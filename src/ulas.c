#include "ulas.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "uldas.h"

FILE *ulasin = NULL;
FILE *ulasout = NULL;
FILE *ulaserr = NULL;
FILE *ulaslstout = NULL;
FILE *ulassymout = NULL;
struct ulas_config ulascfg;
struct ulas ulas;

void ulas_init(struct ulas_config cfg) {
  // init global cfg
  if (ulasin == NULL) {
    ulasin = stdin;
  }
  if (ulasout == NULL) {
    ulasout = stdout;
  }
  if (ulaserr == NULL) {
    ulaserr = stderr;
  }

  ulascfg = cfg;

  // init assembly context
  memset(&ulas, 0, sizeof(ulas));

  ulas.tok = ulas_str(8);

  if (cfg.argc) {
    ulas.filename = cfg.argv[0];
    ulas.initial_filename = cfg.argv[0];
  }

  ulas.pass = ULAS_PASS_FINAL;
  ulas.toks = ulas_tokbuf();
  ulas.exprs = ulas_exprbuf();
  ulas.syms = ulas_symbuf();
  ulas.pp = ulas_preprocinit();
  ulas.scope = 1;

  for (int i = 0; i < ULAS_CHARCODEMAPLEN; i++) {
    ulas.charcodemap[i] = (char)i;
  }
  ulas_arch_set(ULAS_ARCH_SM83);
}

void ulas_nextpass(void) {
  ulas.scope = 1;
  ulas.line = 0;
  ulas.icntr = 0;
  ulas.address = ulascfg.org;
  ulas.chksm = 0;
  ulas.filename = ulas.initial_filename;

  for (int i = 0; i < ULAS_CHARCODEMAPLEN; i++) {
    ulas.charcodemap[i] = (char)i;
  }
}

void ulas_free(void) {
  ulas_strfree(&ulas.tok);
  ulas_tokbuffree(&ulas.toks);
  ulas_exprbuffree(&ulas.exprs);
  ulas_symbuffree(&ulas.syms);
  ulas_preprocfree(&ulas.pp);
}

FILE *ulas_incpathfopen(const char *path, const char *mode) {
  char pathbuf[ULAS_PATHMAX];
  memset(pathbuf, 0, ULAS_PATHMAX);
  unsigned long baselen = strlen(path);

  // check all include paths
  for (int i = 0; i < ulascfg.incpathslen; i++) {
    pathbuf[0] = '\0';
    char *ip = ulascfg.incpaths[i];
    unsigned long len = strlen(ip);
    if (len + baselen + 1 >= ULAS_PATHMAX) {
      continue;
    }

    strlcat(pathbuf, ip, ULAS_PATHMAX);
    if (ip[len - 1] != ULAS_PATHSEP[0]) {
      strlcat(pathbuf, ULAS_PATHSEP, ULAS_PATHMAX);
    }
    strlcat(pathbuf, path, ULAS_PATHMAX);

    FILE *f = fopen(pathbuf, mode);
    if (f != NULL) {
      return f;
    }
  }

  // check the original path last
  FILE *f = fopen(path, mode);
  if (f == NULL) {
    ULASERR("%s: %s\n", path, strerror(errno));
  }

  return f;
}

int ulas_icntr(void) { return ulas.icntr++; }

struct ulas_config ulas_cfg_from_env(void) {
  struct ulas_config cfg;
  memset(&cfg, 0, sizeof(cfg));

  cfg.warn_level = ULAS_WARN_OVERFLOW;

  return cfg;
}

FILE *ulas_fopen(const char *path, const char *mode, FILE *stdfile) {
  if (!path || strncmp(path, ULAS_STDFILEPATH, 1) == 0) {
    return stdfile;
  }
  FILE *f = fopen(path, mode);

  if (!f) {
    ULASPANIC("%s: %s\n", path, strerror(errno));
    return NULL;
  }
  return f;
}

void ulas_fclose(FILE *f) {
  if (!f || f == stdin || f == stdout || f == stderr) {
    return;
  }

  fclose(f);
}

long long ulas_timeusec(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000 * tv.tv_sec + tv.tv_usec;
}

int ulas_main(struct ulas_config cfg) {
  long long total_startusec = ulas_timeusec();
  int rc = 0;
  ulas_init(cfg);
  if (cfg.output_path) {
    ULASDBG("output: %s\n", cfg.output_path);
    ulasout = ulas_fopen(cfg.output_path, "we", stdout);
  }

  if (cfg.sym_path) {
    ULASDBG("symbols: %s\n", cfg.sym_path);
    ulassymout = ulas_fopen(cfg.sym_path, "we", stdout);
  }

  if (cfg.lst_path) {
    ULASDBG("list: %s\n", cfg.lst_path);
    ulaslstout = ulas_fopen(cfg.lst_path, "we", stdout);
  }

  if (cfg.argc > 0) {
    ULASDBG("input: %s\n", cfg.argv[0]);
    ulasin = ulas_fopen(cfg.argv[0], "re", stdin);
  }
  FILE *preprocdst = NULL;

  // only do 2 pass if we have a file as input
  // because  we cannot really rewind stdout
  if (!cfg.preproc_only && ulasin != stdin) {
    ulas.pass = ULAS_PASS_RESOLVE;
  }

  while (ulas.pass >= 0) {
    if (ulascfg.verbose) {
      fprintf(ulaserr, "[Pass %d]\n", ulas.pass);
    }

    ulas_nextpass();

    // FIXME: it would be nice if we could do the 2 pass by clearing the
    // tmpfile instead of making an entierly new one
    if (cfg.preproc_only) {
      preprocdst = ulasout;
    } else {
      preprocdst = tmpfile();
    }

    if (!ulascfg.disas) {
      if (ulas_preproc(preprocdst, ulasin) == -1) {
        rc = -1;
        goto cleanup;
      }
    } else {
      if (ulas_dasm(ulasin, ulasout) == -1) {
        rc = -1;
        goto cleanup;
      }
    }

    if (ulas.pass > ULAS_PASS_FINAL) {
      fclose(preprocdst);
      preprocdst = NULL;
      rewind(ulasin);
    }
    ulas.pass -= 1;
  }

cleanup:
  if (!cfg.preproc_only && preprocdst) {
    ulas_fclose(preprocdst);
  }

  if (cfg.output_path) {
    ulas_fclose(ulasout);
  }

  if (cfg.sym_path) {
    ulas_fclose(ulassymout);
  }

  if (cfg.lst_path) {
    ulas_fclose(ulaslstout);
  }

  if (cfg.argc > 0) {
    ulas_fclose(ulasin);
  }

  ulas_free();

  long long total_endusec = ulas_timeusec();
  if (ulascfg.verbose) {
    fprintf(ulaserr, "[Completed in %lld microseconds]\n",
            total_endusec - total_startusec);
  }

  return rc;
}

int ulas_isname(const char *tok, unsigned long n) {
  if (n == 0) {
    return 0;
  }

  if (isdigit(tok[0])) {
    return 0;
  }

  for (unsigned long i = 0; i < n && tok[i]; i++) {
    char c = tok[i];
    if (c != '_' && !isalnum(c) &&
        !(i == 0 && c == ULAS_TOK_SCOPED_SYMBOL_BEGIN)) {
      return 0;
    }
  }

  return 1;
}

int ulas_islabelname(const char *tok, unsigned long n) {
  return tok[n - 1] == ':' && (ulas_isname(tok, n - 1) || n == 1);
}

struct ulas_sym *ulas_symbolresolve(const char *name, int scope, int *rc) {
  for (int i = 0; i < ulas.syms.len; i++) {
    struct ulas_sym *sym = &ulas.syms.buf[i];
    // when scope is the same as the current one, or scope 0 (global)
    if ((sym->scope == 0 || sym->scope == scope) &&
        strcmp(name, sym->name) == 0) {
      return sym;
    }
  }
  *rc = -1;
  return NULL;
}

int ulas_symbolset(const char *cname, int scope, struct ulas_tok tok,
                   int constant) {
  // remove : from name
  char name[ULAS_SYMNAMEMAX];
  memset(name, 0, ULAS_SYMNAMEMAX);
  unsigned long len = strlen(cname);
  assert(len < ULAS_SYMNAMEMAX);
  strncpy(name, cname, len);
  if (name[len - 1] == ':') {
    name[len - 1] = '\0';
  }

  int rc = 0;
  int resolve_rc = 0;

  // auto-determine scope
  if (scope == -1) {
    if (name[0] == ULAS_TOK_SCOPED_SYMBOL_BEGIN) {
      scope = ulas.scope;
    } else {
      scope = 0;
    }
  }

  struct ulas_sym *existing = ulas_symbolresolve(name, scope, &resolve_rc);
  // inc scope when symbol is global
  if (name[0] != ULAS_TOK_SCOPED_SYMBOL_BEGIN && cname[len - 1] == ':') {
    ulas.scope++;
  }

  if (!existing || (name[0] == '\0' && len == 1)) {
    // def new symbol
    struct ulas_sym new_sym = {strndup(name, len), tok, scope, ulas.pass,
                               constant};
    ulas_symbufpush(&ulas.syms, new_sym);

    rc = ulas_symbolout(ulassymout, &new_sym);
  } else if (existing->lastdefin != ulas.pass || !existing->constant) {
    // redefine if not defined this pass
    existing->lastdefin = ulas.pass;
    ulas_tokfree(&existing->tok);
    existing->tok = tok;
    existing->constant = constant;

    rc = ulas_symbolout(ulassymout, existing);
  } else {
    // exists.. cannot have duplicates!
    rc = -1;
    ULASERR("Redefenition of symbol '%s' in scope %d\n", name, scope);
  }
  return rc;
}

int ulas_symbolout_mlbloc(FILE *dst, long addr) {
  if (addr == -1) {
    fprintf(dst, "Unknown:");
    return 0;
  }
  
  // TODO: maybe allow the user to define this by using 
  // .section and just trust the label location in the source
  // is correct
  switch (ulas.arch.type) {
  case ULAS_ARCH_SM83:
    if (addr >= 0x0000 && addr <= 0x7FFF) {
      fprintf(dst, "GbPrgRom:");
      return 0;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
      fprintf(dst, "GbWorkRam:");
      return 0xC000;
    } else {
      fprintf(dst, "Unknown:");
      return 0;
    }
    break;
  }
  return 0;
}

int ulas_symbolout(FILE *dst, struct ulas_sym *s) {
  if (!dst || ulas.pass != ULAS_PASS_FINAL) {
    return 0;
  }

  int rc = 0;

  switch (ulascfg.sym_fmt) {
  case ULAS_SYM_FMT_DEFAULT:
    if (!s->name || s->name[0] == '\0') {
      fprintf(dst, "<unnamed> = ");
    } else {
      fprintf(dst, "%s = ", s->name);
    }
    switch (s->tok.type) {
    case ULAS_INT:
      fprintf(dst, "0x%x", ulas_valint(&s->tok, &rc));
      break;
    case ULAS_STR:
      fprintf(dst, "%s", ulas_valstr(&s->tok, &rc));
      break;
    default:
      fprintf(dst, "<Unknown type>");
      break;
    }
    fprintf(dst, "\n");
    break;
  case ULAS_SYM_FMT_MLB:
    switch (s->tok.type) {
    case ULAS_INT: {
      int valint = ulas_valint(&s->tok, &rc);
      valint -= ulas_symbolout_mlbloc(dst, valint);
      fprintf(dst, "%x:", valint);
      break;
    }
    case ULAS_STR:
      ulas_symbolout_mlbloc(dst, -1);
      fprintf(dst, "%s:", ulas_valstr(&s->tok, &rc));
      break;
    default:
      ulas_symbolout_mlbloc(dst, -1);
      fprintf(dst, "<Unknown type>:");
      break;
    }
    if (!s->name || s->name[0] == '\0') {
      fprintf(dst, "<unnamed>:<unnamed>");
    } else {
      fprintf(dst, "%s:%s", s->name, s->name);
    }

    fprintf(dst, "\n");
    break;
  }

  return rc;
}

#define ULAS_TOKISTERM write
#define ULAS_TOKCOND (i < n && write < n && line[i])

int ulas_tok(struct ulas_str *dst, const char **out_line, unsigned long n) {
  const char *line = *out_line;
  ulas_strensr(dst, n + 1);

  int i = 0;
  int write = 0;

  // always skip leading terminators
  while (ULAS_TOKCOND && isspace(line[i])) {
    i++;
  }

  // string token
  if (line[i] == '"') {
    dst->buf[write++] = line[i++];
    int last_escape = 0;
    while (ULAS_TOKCOND && (line[i] != '\"' || last_escape)) {
      last_escape = line[i] == '\\';
      dst->buf[write++] = line[i];
      i++;
    }
    dst->buf[write++] = line[i++];
  } else {
    while (ULAS_TOKCOND) {
      char c = line[i];

      switch (c) {
      case '+':
      case '-':
      case '*':
      case '/':
      case '~':
      case '|':
      case '&':
      case '%':
      case '(':
      case ')':
      case '[':
      case ']':
      case ',':
      case '\\':
      case ULAS_TOK_COMMENT:
        if (ULAS_TOKISTERM) {
          goto tokdone;
        }
        // single char tokens
        dst->buf[write++] = line[i++];
        goto tokdone;
      case '$':
        if (ULAS_TOKISTERM) {
          goto tokdone;
        }
        // special var for preprocessor
        // make sure we have enough space in buffer
        ulas_strensr(dst, write + 2);
        // escape char tokens
        dst->buf[write++] = line[i++];
        if (line[i] == '$') {
          dst->buf[write++] = line[i++];
        } else {
          while (line[i] && isdigit(line[i])) {
            dst->buf[write++] = line[i++];
          }
        }
        goto tokdone;
      case '=':
      case '<':
      case '!':
      case '>':
        if (line[i + 1] == '=' || line[i + 1] == '>' || line[i + 1] == '<') {
          dst->buf[write] = line[i];
          i++;
          write++;
        }
        dst->buf[write] = line[i];
        write++;
        i++;
        goto tokdone;
      default:
        if (isspace(line[i])) {
          goto tokdone;
        }
        dst->buf[write] = line[i];
        write++;
        break;
      }
      i++;
    }
  }
tokdone:

  dst->buf[write] = '\0';

  *out_line += i;
  return i;
}

// tokenize until the char c is seen
// this will also consume c and out_line will point to the next valid char
// this is useful if the next expected token is well defined and we want to
// capture everything between
// will remove leading white spaces
int ulas_tokuntil(struct ulas_str *dst, char c, const char **out_line,
                  unsigned long n) {
  const char *line = *out_line;
  ulas_strensr(dst, n + 1);

  int i = 0;
  int write = 0;

  while (ULAS_TOKCOND && isspace(line[i])) {
    i++;
  }

  while (ULAS_TOKCOND && line[i] != c) {
    dst->buf[write++] = line[i++];
  }

  if (line[i] == c) {
    i++;
  }

  dst->buf[write] = '\0';

  *out_line += i;
  return i;
}

int ulas_unescape(char c, int *rc) {
  switch (c) {
  case '\'':
  case '\\':
  case '"':
    return c;
  case 'n':
    return '\n';
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'r':
    return '\r';
  case '?':
    return '\?';
  case '0':
    return '\0';
  default:
    ULASERR("Unexpected esxcape sequence: \\%c\n", c);
    *rc = -1;
    break;
  }

  return '\0';
}

struct ulas_tok ulas_totok(char *buf, unsigned long n, int *rc) {
  struct ulas_tok tok;
  memset(&tok, 0, sizeof(tok));

  if (n == 0) {
    *rc = -1;
    goto end;
  }

  unsigned char first = buf[0];
  buf++;

  if (n == 1 && !isalnum(first)) {
    // single char tokens
    tok.type = first;
  } else {
    switch (first) {
    case '"':
      // string
      tok.type = ULAS_STR;

      // FIXME: this likely mallocs a few extra bytes
      // but honestly its probably fine
      tok.val.strv = malloc(n * sizeof(char) + 1);
      memset(tok.val.strv, 0, n);

      long i = 0;
      while (*buf && *buf != '\"') {
        if (*buf == '\\') {
          buf++;
          tok.val.strv[i] = (char)ulas_unescape(*buf, rc);
        } else {
          tok.val.strv[i] = *buf;
        }
        i++;
        buf++;
      }
      tok.val.strv[i] = '\0';

      if (*buf != '\"') {
        *rc = -1;
        ULASERR("Unterminated string sequence\n");
        goto end;
      }
      buf++;
      break;
    case '=':
      if (*buf == '=') {
        tok.type = ULAS_EQ;
        buf++;
      }
      break;
    case '!':
      if (*buf == '=') {
        tok.type = ULAS_NEQ;
        buf++;
      }
      break;
    case '<':
      if (*buf == '=') {
        tok.type = ULAS_LTEQ;
        buf++;
      } else if (*buf == '<') {
        tok.type = ULAS_LSHIFT;
        buf++;
      }
      break;
    case '>':
      if (*buf == '=') {
        tok.type = ULAS_GTEQ;
        buf++;
      } else if (*buf == '>') {
        tok.type = ULAS_RSHIFT;
        buf++;
      }
      break;
    default:
      if (isdigit(first)) {
        // integer
        tok.type = ULAS_INT;

        // 0b prefix is not supported in strtol... so we implement it by hand
        if (*buf == 'b') {
          buf++;
          tok.val.intv = (int)strtol(buf, &buf, 2);
        } else {
          tok.val.intv = (int)strtol(buf - 1, &buf, 0);
        }
      } else if (first == '\'') {
        tok.type = ULAS_INT;
        if (*buf == '\\') {
          buf++;
          tok.val.intv = ulas_unescape(*buf, rc);
        } else {
          tok.val.intv = (int)*buf;
        }
        buf++;
        if (*buf != '\'') {
          *rc = -1;
          ULASERR("Unterminated character sequence\n");
          goto end;
        }
        buf++;
        break;
      } else if (ulas_isname(buf - 1, n)) {
        // literal token
        // we resolve it later, will need to malloc here for now
        tok.type = ULAS_SYMBOL;
        tok.val.strv = strndup(buf - 1, n);
        buf += n - 1;
      } else {
        ULASERR("Unexpected token: %s\n", buf);
        *rc = -1;
        goto end;
      }
      break;
    }
  }

end:
  // did we consume the entire token?
  if (buf[0] != '\0') {
    *rc = -1;
  }

  return tok;
}

#undef ULAS_TOKCOND
#undef ULAS_TOKISTERM

struct ulas_str ulas_str(unsigned long n) {
  struct ulas_str str = {malloc(n), n};
  return str;
}

struct ulas_str ulas_strensr(struct ulas_str *s, unsigned long maxlen) {
  if (s->maxlen < maxlen) {
    char *c = realloc(s->buf, maxlen);
    if (!c) {
      ULASPANIC("%s\n", strerror(errno));
    }
    s->maxlen = maxlen;
    s->buf = c;
  }

  return *s;
}

struct ulas_str ulas_strreq(struct ulas_str *s, unsigned long n) {
  return ulas_strensr(s, strnlen(s->buf, s->maxlen) + n);
}

void ulas_strfree(struct ulas_str *s) {
  if (s->buf) {
    free(s->buf);
  }
}

struct ulas_ppdef *ulas_preprocgetdef(struct ulas_preproc *pp, const char *name,
                                      unsigned long maxlen) {
  for (unsigned long i = 0; i < pp->defslen; i++) {
    struct ulas_ppdef *def = &pp->defs[i];
    if (!def->undef && strncmp(def->name, name, maxlen) == 0) {
      return def;
    }
  }

  return NULL;
}

// inserts all leading white space from praw_line into linebuf
int ulas_preproclws(struct ulas_preproc *pp, const char *praw_line,
                    unsigned long maxlen) {
  int i = 0;
  while (i < maxlen && praw_line[i] && isspace(praw_line[i])) {
    i++;
  }

  ulas_strreq(&pp->line, i + 1);
  strncat(pp->line.buf, praw_line, i);
  return i;
}

void ulas_trimend(char c, char *buf, unsigned long n) {
  unsigned long buflen = strnlen(buf, n);
  if (buflen == 0) {
    return;
  }
  // remove trailing new line if present
  while (buf[buflen - 1] == '\n') {
    buf[buflen - 1] = '\0';
    buflen--;
  }
}

char *ulas_preprocexpand(struct ulas_preproc *pp, const char *raw_line,
                         unsigned long *n) {
  const char *praw_line = raw_line;
  memset(pp->line.buf, 0, pp->line.maxlen);

  int read = 0;
  int first_tok = 1;
  int skip_next_tok = 0;
  int comment_seen = 0;

  // go through all tokens, see if a define matches the token,
  // if so expand it
  // only expand macros if they match toks[0] though!
  // otherwise memcpy the read bytes 1:1 into the new string
  while ((read = ulas_tok(&pp->tok, &praw_line, *n))) {
    struct ulas_ppdef *def =
        ulas_preprocgetdef(pp, pp->tok.buf, pp->tok.maxlen);

    // if it is the first token, and it begins with a # do not process at all!
    // if the first token is a # preproc directive skip the second token at all
    // times
    if ((first_tok && pp->tok.buf[0] == ULAS_TOK_PREPROC_BEGIN) ||
        skip_next_tok) {
      def = NULL;
      skip_next_tok = !skip_next_tok;
    } else if (pp->tok.buf[0] == ULAS_TOK_COMMENT) {
      // if its a comment at the end of a preproc statement
      // just bail now
      comment_seen = 1;
    }
    first_tok = 0;

    if (def && !comment_seen) {
      // if so... expand now and leave
      switch (def->type) {
      case ULAS_PPDEF: {
        unsigned long val_len = strlen(def->value);
        int wsi = ulas_preproclws(pp, praw_line - read, *n);
        if (val_len) {
          // make sure to include leading white space
          // adjust total length
          *n -= strlen(pp->tok.buf);
          *n += val_len;
          ulas_strensr(&pp->line, (*n) + 1 + wsi);

          // only remove the first white space char if the lenght of value
          // is greater than 1, otherwise just leave it be...
          if (val_len > 1) {
            strncat(pp->line.buf, def->value + 1, val_len - 1);
          } else {
            strncat(pp->line.buf, def->value, val_len);
          }
        }
        break;
      }
      case ULAS_PPMACRO: {
        // TODO: i am sure we can optimize the resize of line buffers here...
        // TODO: allow recursive macro calls

        // get 9 comma separated values.
        // $1-$9 will reference the respective arg
        // $0 will reference the entire line after the macro name
        // there can be more than 9 args, but anything after the 9th arg can
        // only be accessed via $0
        const char *line = praw_line;
        unsigned long linelen = strlen(praw_line);
        // clear all params from previous attempt
        for (unsigned long i = 0; i < ULAS_MACROPARAMMAX; i++) {
          pp->macroparam[i].buf[0] = '\0';
        }

        // loop until 9 args are found or the line ends
        int paramc = 0;
        while (paramc < ULAS_MACROPARAMMAX &&
               // TODO: allow escaping , with \,
               ulas_tokuntil(&pp->macroparam[paramc], ',', &praw_line, *n) >
                   0) {
          // trim new lines from the end of macro params
          ulas_trimend('\n', pp->macroparam[paramc].buf,
                       strlen(pp->macroparam[paramc].buf));
          paramc++;
        }
        ulas_strensr(&pp->line, strlen(def->value) + 2);

        const char *macro_argname[ULAS_MACROPARAMMAX] = {
            "$1", "$2",  "$3",  "$4",  "$5",  "$6",  "$7", "$8",
            "$9", "$10", "$11", "$12", "$13", "$14", "$15"};

        const char *val = def->value;
        unsigned long vallen = strlen(def->value);
        unsigned long valread = 0;

        // the pointer to tocat will be the variable's value if any
        // exists
        const char *tocat = NULL;
        unsigned long tocatlen = 0;

        // now tokenize the macro's value and look for $0-$9
        // and replace those instances
        // eveyrthing else will just be copied as is
        while ((valread = ulas_tok(&pp->macrobuf, &val, vallen)) > 0) {
          tocat = NULL;
          char numbuf[128];

          // decide what tocat should be
          for (unsigned long mi = 0; mi < ULAS_MACROPARAMMAX; mi++) {
            const char *name = macro_argname[mi];
            if (pp->macroparam[mi].buf[0] &&
                strncmp((name), pp->macrobuf.buf, pp->macrobuf.maxlen) == 0) {
              ulas_strensr(&pp->line, strnlen(pp->line.buf, pp->line.maxlen) +
                                          strnlen(pp->macroparam[mi].buf,
                                                  pp->macroparam[mi].maxlen) +
                                          1);

              tocat = pp->macroparam[mi].buf;
              tocatlen = pp->macroparam[mi].maxlen;

              break;
            }
          }

          if (linelen &&
              strncmp("$0", pp->macrobuf.buf, pp->macrobuf.maxlen) == 0) {
            ulas_strensr(&pp->line,
                         strnlen(pp->line.buf, pp->line.maxlen) + linelen + 1);

            if (linelen > 1) {
              // this skips the separating token which is usually a space
              // all further spaces are included though!
              tocat = line + 1;
              tocatlen = linelen - 1;
            } else {
              // do not do this if the line is literally empty!
              tocat = line;
              tocatlen = linelen;
            }
          } else if (linelen && strncmp("$$", pp->macrobuf.buf,
                                        pp->macrobuf.maxlen) == 0) {
            sprintf(numbuf, "%x", ulas_icntr());
            ulas_strensr(&pp->line,
                         strnlen(pp->line.buf, pp->line.maxlen) + 128 + 1);
            tocat = numbuf;
            tocatlen = 128;
          }

          if (!tocat) {
            ulas_strensr(&pp->line, strlen(pp->line.buf) + valread + 1);
            strncat(pp->line.buf, val - valread, valread);
          } else {
            // make sure to include leading white space
            int wsi = ulas_preproclws(pp, val - valread, vallen);
            ulas_strensr(&pp->line, strnlen(pp->line.buf, pp->line.maxlen) +
                                        tocatlen + wsi + 1);
            strncat(pp->line.buf, tocat, tocatlen);
          }
        }
        goto end;
      }
      }

    } else {
      // if not found: copy everythin from prev to the current raw_line point -
      // tok lenght -> this keeps the line in-tact as is
      ulas_strensr(&pp->line, (*n) + 1);
      strncat(pp->line.buf, praw_line - read, read);
    }
  }

end:
  *n = strlen(pp->line.buf);
  return pp->line.buf;
}

int ulas_preprocdef(struct ulas_preproc *pp, struct ulas_ppdef def) {
  pp->defslen++;
  void *defs = realloc(pp->defs, pp->defslen * sizeof(struct ulas_ppdef));
  if (!defs) {
    ULASPANIC("%s\n", strerror(errno));
  }
  pp->defs = defs;
  pp->defs[pp->defslen - 1] = def;
  return 0;
}

int ulas_preprocline(struct ulas_preproc *pp, FILE *dst, FILE *src,
                     const char *raw_line, unsigned long n) {
  /**
   * Footgun warning:
   * We do use raw pointers to the line here... which is fine
   * as long as it is never used after a recursive call to the preprocessor!
   * never use this pointer after such a recursive call!
   */
  char *line = ulas_preprocexpand(pp, raw_line, &n);
  const char *pline = line;

  const char *dirstrs[] = {
      ULAS_PPSTR_DEF,    ULAS_PPSTR_MACRO,   ULAS_PPSTR_IFDEF,
      ULAS_PPSTR_IFNDEF, ULAS_PPSTR_ENDIF,   ULAS_PPSTR_ENDMACRO,
      ULAS_PPSTR_UNDEF,  ULAS_PPSTR_INCLUDE, NULL};
  enum ulas_ppdirs dirs[] = {ULAS_PPDIR_DEF,   ULAS_PPDIR_MACRO,
                             ULAS_PPDIR_IFDEF, ULAS_PPDIR_IFNDEF,
                             ULAS_PPDIR_ENDIF, ULAS_PPDIR_ENDMACRO,
                             ULAS_PPDIR_UNDEF, ULAS_PPDIR_INCLUDE};

  enum ulas_ppdirs found_dir = ULAS_PPDIR_NONE;

  // check if the first token is any of the valid preproc directives
  if (ulas_tok(&pp->tok, &pline, n)) {
    // not a preproc directive...
    if (pp->tok.buf[0] != ULAS_TOK_PREPROC_BEGIN) {
      goto found;
    }
    for (unsigned long i = 0; dirstrs[i]; i++) {
      if (strncmp(dirstrs[i], pp->tok.buf, pp->tok.maxlen) == 0) {
        found_dir = dirs[i];
        goto found;
      }
    }

    ULASPANIC("Unknown preprocessor directive: %s\n", line);
    return -1;
  }
found:

  if (found_dir != ULAS_PPDIR_NONE) {
    ulas_trimend('\n', line, strlen(line));
    switch (found_dir) {
    case ULAS_PPDIR_DEF: {
      // next token is a name
      // and then the entire remainder of the line is a value
      if (ulas_tok(&pp->tok, &pline, n) == 0) {
        ULASERR("Expected name for #define\n");
        return -1;
      }

      if (!ulas_isname(pp->tok.buf, strlen(pp->tok.buf))) {
        ULASERR("'%s' is not a valid #define name!\n", pp->tok.buf);
        return -1;
      }

      struct ulas_ppdef def = {ULAS_PPDEF, strdup(pp->tok.buf), strdup(pline),
                               0};
      ulas_preprocdef(pp, def);
      // define short-circuits the rest of the logic
      // because it just takes the entire rest of the line as a value!
      goto dirdone;
    }
    case ULAS_PPDIR_MACRO: {
      // get a name, ensure no more tokens come after
      // and then consume lines until ENDMACRO is seen
      if (ulas_tok(&pp->tok, &pline, n) == 0) {
        ULASERR("Expected name for #macro\n");
        return -1;
      }

      if (!ulas_isname(pp->tok.buf, strlen(pp->tok.buf))) {
        ULASERR("'%s' is not a valid #macro name!\n", pp->tok.buf);
        return -1;
      }
      char *name = strdup(pp->tok.buf);

      struct ulas_str val = ulas_str(32);
      memset(val.buf, 0, 32);

      char buf[ULAS_LINEMAX];
      memset(buf, 0, ULAS_LINEMAX);

      // consume lines until #endmacro is read
      // if reaching end of input without #endmacro we have an unterminated
      // macro. pass NULL as dst to consume lines that are being read instead of
      // echoing them back
      int rc = 0;
      while ((rc = ulas_preprocnext(pp, NULL, src, buf, ULAS_LINEMAX)) > 0) {
        if (rc == ULAS_PPDIR_ENDMACRO) {
          // we need to clear the line buffer to now echo back
          // the #endmacro directive
          pp->line.buf[0] = '\0';
          break;
        }

        unsigned long len = strnlen(pp->line.buf, pp->line.maxlen);
        ulas_strensr(&val, strnlen(val.buf, val.maxlen) + len + 1);
        strncat(val.buf, pp->line.buf, val.maxlen);
      }

      if (rc != ULAS_PPDIR_ENDMACRO) {
        ULASERR("Unterminated macro directive\n");
        ulas_strfree(&val);
        free(name);
        return -1;
      }
      // we leak the str's buffer into the def now
      // this is ok because we call free for it later anyway
      struct ulas_ppdef def = {ULAS_PPMACRO, name, val.buf, 0};
      ulas_preprocdef(pp, def);

      goto dirdone;
    }
    case ULAS_PPDIR_ENDIF:
    case ULAS_PPDIR_ENDMACRO:
      break;
    case ULAS_PPDIR_IFDEF:
    case ULAS_PPDIR_IFNDEF: {
      // get the name
      // get a name, ensure no more tokens come after
      // and then consume lines until ENDMACRO is seen
      if (ulas_tok(&pp->tok, &pline, n) == 0) {
        ULASERR("Expected name for #if(n)def\n");
        return -1;
      }
      struct ulas_ppdef *def =
          ulas_preprocgetdef(pp, pp->tok.buf, pp->tok.maxlen);

      char buf[ULAS_LINEMAX];
      memset(buf, 0, ULAS_LINEMAX);

      FILE *defdst = NULL;
      if ((def && found_dir == ULAS_PPDIR_IFDEF) ||
          (!def && found_dir == ULAS_PPDIR_IFNDEF)) {
        defdst = dst;
      }
      // loop until end of line or endif
      int rc = 0;
      while ((rc = ulas_preprocnext(pp, defdst, src, buf, ULAS_LINEMAX)) > 0) {
        if (rc == ULAS_PPDIR_ENDIF) {
          // we need to clear the line buffer to now echo back
          // the #endif directive
          pp->line.buf[0] = '\0';
          break;
        }
      }

      if (rc != ULAS_PPDIR_ENDIF) {
        ULASERR("Unterminated if(n)def directive\n");
        return -1;
      }
      goto dirdone;
    }
    case ULAS_PPDIR_UNDEF: {
      if (ulas_tok(&pp->tok, &pline, n) == 0) {
        ULASERR("Expected name for #undef\n");
        return -1;
      }
      struct ulas_ppdef *def = NULL;
      while ((def = ulas_preprocgetdef(pp, pp->tok.buf, pp->tok.maxlen))) {
        def->undef = 1;
      }

      break;
    }
    case ULAS_PPDIR_INCLUDE: {
      int rc = found_dir;
      char *path = ulas_strexpr(&pline, strlen(pline), &rc);
      if (rc == -1 || !path) {
        return rc;
      }
      FILE *f = ulas_incpathfopen(path, "re");
      if (!f) {
        return -1;
      }
      char *prev_path = ulas.filename;
      unsigned long prev_lines = ulas.line;

      ulas.filename = strdup(path);
      ulas.line = 0;

      FILE *tmp = tmpfile();
      rc = ulas_preproc(tmp, f);
      // only error if -1
      if (rc != -1) {
        rc = found_dir;
      }

      free(ulas.filename);
      ulas.filename = prev_path;
      ulas.line = prev_lines;

      fclose(f);
      fclose(tmp);
      return rc;
    }
    default:
      // this should not happen!
      break;
    }

  dirdone:
    return found_dir;
  } else if (dst) {
    fwrite(line, 1, n, dst);
  }

  return ULAS_PPDIR_NONE;
}

int ulas_preprocnext(struct ulas_preproc *pp, FILE *dst, FILE *src, char *buf,
                     int n) {
  int rc = 1;
  if (fgets(buf, n, src) != NULL) {
    ulas.line++;

    unsigned long buflen = strlen(buf);

    rc = ulas_preprocline(pp, dst, src, buf, buflen);
  } else {
    rc = 0;
  }

  return rc;
}

struct ulas_preproc ulas_preprocinit(void) {
  struct ulas_preproc pp = {NULL, 0, ulas_str(1), ulas_str(1)};
  for (unsigned long i = 0; i < ULAS_MACROPARAMMAX; i++) {
    pp.macroparam[i] = ulas_str(8);
  }
  pp.macrobuf = ulas_str(8);
  return pp;
}

void ulas_preprocclear(struct ulas_preproc *pp) {
  for (unsigned long i = 0; i < pp->defslen; i++) {
    if (pp->defs[i].name) {
      free(pp->defs[i].name);
    }
    if (pp->defs[i].value) {
      free(pp->defs[i].value);
    }
  }

  pp->defslen = 0;
}

void ulas_preprocfree(struct ulas_preproc *pp) {
  ulas_strfree(&pp->line);
  ulas_strfree(&pp->tok);

  ulas_preprocclear(pp);

  for (unsigned long i = 0; i < ULAS_MACROPARAMMAX; i++) {
    ulas_strfree(&pp->macroparam[i]);
  }
  ulas_strfree(&pp->macrobuf);

  if (pp->defs) {
    free(pp->defs);
  }
}

int ulas_preproc(FILE *dst, FILE *src) {
  FILE *asmsrc = dst;
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);
  int rc = 0;

  // init

  long prevseek = ftell(asmsrc);
  // preproc
  while ((rc = ulas_preprocnext(&ulas.pp, dst, src, buf, ULAS_LINEMAX)) > 0) {
    if (ulascfg.preproc_only) {
      continue;
    }
    // after each preproc line we assembly it by reading it back
    // from the temporary buffer
    fseek(asmsrc, prevseek, SEEK_SET);
    if (ulas_asm(ulasout, asmsrc) == -1) {
      rc = -1;
      goto fail;
    }
    prevseek = ftell(asmsrc);
  }

fail:
  return rc;
}

/**
 * Literals, tokens and expressions
 */

int ulas_valint(struct ulas_tok *lit, int *rc) {
  if (ulas.pass != ULAS_PASS_FINAL) {
    return 0;
  }

  if (lit->type == ULAS_SYMBOL) {
    struct ulas_sym *stok = ulas_symbolresolve(lit->val.strv, ulas.scope, rc);
    if (!stok || *rc == -1) {
      ULASERR("Unabel to resolve '%s'\n", lit->val.strv);
      *rc = -1;
      return 0;
    }
    return ulas_valint(&stok->tok, rc);
  }

  if (!lit || (lit->type != ULAS_INT && lit->type != ULAS_TOK_CURRENT_ADDR)) {
    ULASERR("Expected int\n");
    *rc = -1;
    return 0;
  }

  if (lit->type == ULAS_TOK_CURRENT_ADDR) {
    return (int)ulas.address;
  }

  return lit->val.intv;
}

char *ulas_valstr(struct ulas_tok *lit, int *rc) {
  if (!lit || lit->type != ULAS_STR) {
    ULASERR("Expected str\n");
    *rc = -1;
    return NULL;
  }

  return lit->val.strv;
}

struct ulas_tokbuf ulas_tokbuf(void) {
  struct ulas_tokbuf tb;
  memset(&tb, 0, sizeof(tb));

  tb.maxlen = 5;
  tb.buf = malloc(tb.maxlen * sizeof(struct ulas_tok));
  tb.len = 0;

  return tb;
}

struct ulas_tok *ulas_tokbufget(struct ulas_tokbuf *tb, int i) {
  if (i >= tb->len) {
    return NULL;
  }

  return &tb->buf[i];
}

int ulas_tokbufpush(struct ulas_tokbuf *tb, struct ulas_tok tok) {
  if (tb->len >= tb->maxlen) {
    tb->maxlen += 5;
    void *n = realloc(tb->buf, tb->maxlen * sizeof(struct ulas_tok));
    if (!n) {
      ULASPANIC("%s\n", strerror(errno));
    }

    tb->buf = n;
  }

  tb->buf[tb->len] = tok;
  return (int)tb->len++;
}

void ulas_tokfree(struct ulas_tok *t) {
  if (t->type == ULAS_SYMBOL || t->type == ULAS_STR) {
    free(t->val.strv);
  }
}

void ulas_tokbufclear(struct ulas_tokbuf *tb) {
  for (long i = 0; i < tb->len; i++) {
    struct ulas_tok *t = &tb->buf[i];
    ulas_tokfree(t);
  }
  tb->len = 0;
}

void ulas_tokbuffree(struct ulas_tokbuf *tb) {
  ulas_tokbufclear(tb);
  free(tb->buf);
}

struct ulas_exprbuf ulas_exprbuf(void) {
  struct ulas_exprbuf eb;
  memset(&eb, 0, sizeof(eb));

  eb.maxlen = 10;
  eb.buf = malloc(sizeof(struct ulas_expr) * eb.maxlen);

  return eb;
}

struct ulas_expr *ulas_exprbufget(struct ulas_exprbuf *eb, int i) {
  if (i >= eb->len) {
    return NULL;
  }

  return &eb->buf[i];
}

int ulas_exprbufpush(struct ulas_exprbuf *eb, struct ulas_expr expr) {
  if (eb->len >= eb->maxlen) {
    eb->maxlen *= 2;
    void *newbuf = realloc(eb->buf, eb->maxlen * sizeof(struct ulas_expr));
    if (!newbuf) {
      ULASPANIC("%s\n", strerror(errno));
    }
    eb->buf = newbuf;
  }

  eb->buf[eb->len] = expr;
  return (int)eb->len++;
}

void ulas_exprbufclear(struct ulas_exprbuf *eb) { eb->len = 0; }

void ulas_exprbuffree(struct ulas_exprbuf *eb) { free(eb->buf); }

struct ulas_symbuf ulas_symbuf(void) {
  struct ulas_symbuf sb;
  memset(&sb, 0, sizeof(sb));

  sb.maxlen = 10;
  sb.buf = malloc(sizeof(struct ulas_sym) * sb.maxlen);

  return sb;
}

int ulas_symbufpush(struct ulas_symbuf *sb, struct ulas_sym sym) {
  if (sb->len >= sb->maxlen) {
    sb->maxlen *= 2;
    void *newbuf = realloc(sb->buf, sb->maxlen * sizeof(struct ulas_sym));
    if (!newbuf) {
      ULASPANIC("%s\n", strerror(errno));
    }
    sb->buf = newbuf;
  }

  sb->buf[sb->len] = sym;
  return (int)sb->len++;
}

struct ulas_sym *ulas_symbufget(struct ulas_symbuf *sb, int i) {
  if (i >= sb->len) {
    return NULL;
  }

  return &sb->buf[i];
}

void ulas_symbufclear(struct ulas_symbuf *sb) {
  for (long i = 0; i < sb->len; i++) {
    struct ulas_sym *s = &sb->buf[i];
    free(s->name);
  }
  sb->len = 0;
}

void ulas_symbuffree(struct ulas_symbuf *sb) {
  ulas_symbufclear(sb);
  free(sb->buf);
}

/**
 * Assembly step
 */

int ulas_istokend(struct ulas_str *tok) {
  long len = (int)strnlen(tok->buf, tok->maxlen);
  // skip comments though, they are not trailing tokens!
  if (len > 0 && tok->buf[0] != ULAS_TOK_COMMENT) {
    return 0;
  }

  return 1;
}

// tokenize all until a terminator token or comment is reached
int ulas_tokexpr(const char **line, unsigned long n) {
  ulas_tokbufclear(&ulas.toks);

  int tokrc = 0;
  while ((tokrc = ulas_tok(&ulas.tok, line, n) > 0)) {
    if (tokrc == -1) {
      goto end;
    }

    // empty tokens are going to be ignored
    if (strnlen(ulas.tok.buf, ulas.tok.maxlen) == 0) {
      continue;
    }

    // interpret the token
    struct ulas_tok tok = ulas_totok(
        ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), &tokrc);
    if (tokrc == -1) {
      goto end;
    }

    // check for any expression terminators here
    if (tok.type == ',' || tok.type == ']' || tok.type == '=' ||
        ulas_istokend(&ulas.tok)) {
      // on terminator we roll back line so that the terminator token
      // is not consumed now
      *line -= tokrc;
      goto end;
    }

    // now we can loop token type, add all tokens to the token buffer
    ulas_tokbufpush(&ulas.toks, tok);
  }

end:
  return tokrc;
}

/**
 * Parse expressions from tokens
 * return tree-like structure
 * all these functions return an int index into
 * the expression buffer.
 * they also all take an index variable i which can be freely modified.
 * i may never exceed toks->len.
 * if i is not the same as toks->len when the parser finishes
 * we error out because of trailing tokens!
 */

int ulas_parseexprat(int *i);

int ulas_parseprim(int *i) {
  struct ulas_tok *t = ulas_tokbufget(&ulas.toks, *i);
  if (!t ||
      (t->type != ULAS_INT && t->type != ULAS_STR && t->type != ULAS_SYMBOL &&
       t->type != ULAS_TOK_CURRENT_ADDR && t->type != '(' && t->type != ')')) {
    ULASERR("Primary expression expected\n");
    return -1;
  }

  if (t->type == '(') {
    *i += 1;
    int head = ulas_parseexprat(i);

    struct ulas_expgrp grp = {head};
    union ulas_expval val = {.grp = grp};
    struct ulas_expr e = {ULAS_EXPGRP, val};

    struct ulas_tok *closing = ulas_tokbufget(&ulas.toks, *i);
    if (!closing || closing->type != ')') {
      ULASERR("Unterminated group expression\n");
      return -1;
    }
    *i += 1;
    return ulas_exprbufpush(&ulas.exprs, e);
  } else {
    struct ulas_expprim prim = {*i};
    union ulas_expval val = {.prim = prim};
    struct ulas_expr e = {ULAS_EXPPRIM, val};
    *i += 1;
    return ulas_exprbufpush(&ulas.exprs, e);
  }
}

int ulas_parsecmp(int *i);

int ulas_parseun(int *i) {
  struct ulas_tok *t = ulas_tokbufget(&ulas.toks, *i);

  if (t &&
      (t->type == '!' || t->type == '-' || t->type == '~' || t->type == '+')) {
    int op = *i;
    *i += 1;
    int right = ulas_parseun(i);

    struct ulas_expun un = {right, op};
    union ulas_expval val = {.un = un};
    struct ulas_expr e = {ULAS_EXPUN, val};
    return ulas_exprbufpush(&ulas.exprs, e);
  }

  return ulas_parseprim(i);
}

int ulas_parsefact(int *i) {
  int expr = ulas_parseun(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == '*' || t->type == '/' || t->type == '%')) {
    int op = *i;
    *i += 1;
    int right = ulas_parseun(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parseterm(int *i) {
  int expr = ulas_parsefact(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == '+' || t->type == '-')) {
    int op = *i;
    *i += 1;
    int right = ulas_parsefact(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parseshift(int *i) {
  int expr = ulas_parseterm(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == ULAS_RSHIFT || t->type == ULAS_LSHIFT)) {
    int op = *i;
    *i += 1;
    int right = ulas_parseterm(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parsecmp(int *i) {
  int expr = ulas_parseshift(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == ULAS_LTEQ || t->type == ULAS_GTEQ || t->type == '>' ||
          t->type == '<')) {
    int op = *i;
    *i += 1;
    int right = ulas_parseshift(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parseeq(int *i) {
  int expr = ulas_parsecmp(i);
  struct ulas_tok *t = NULL;
  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == ULAS_EQ || t->type == ULAS_NEQ)) {
    int op = *i;
    *i += 1;
    int right = ulas_parsecmp(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parsebit(int *i) {
  int expr = ulas_parseeq(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == '&' || t->type == '|' || t->type == '^')) {
    int op = *i;
    *i += 1;
    int right = ulas_parseeq(i);

    struct ulas_expbin bin = {expr, right, op};
    union ulas_expval val = {.bin = bin};
    struct ulas_expr e = {ULAS_EXPBIN, val};
    expr = ulas_exprbufpush(&ulas.exprs, e);
  }

  return expr;
}

int ulas_parseexprat(int *i) { return ulas_parsebit(i); }

// parses tokens to expression tree
// returns head expression index
int ulas_parseexpr(void) {
  ulas_exprbufclear(&ulas.exprs);

  struct ulas_tokbuf *toks = &ulas.toks;
  if (toks->len == 0) {
    ULASERR("Expected expression\n");
    return -1;
  }

  int i = 0;
  int rc = ulas_parseexprat(&i);

  if (i < toks->len) {
    ULASERR("Trailing token at index %d\n", i);
    rc = -1;
  }

  return rc;
}

int ulas_intexpreval(int i, int *rc) {
  struct ulas_expr *e = ulas_exprbufget(&ulas.exprs, i);
  if (!e) {
    ULASERR("unable to evaluate expression\n");
    *rc = -1;
    return 0;
  }

  switch ((int)e->type) {
  case ULAS_EXPBIN: {
    struct ulas_tok *op = ulas_tokbufget(&ulas.toks, (int)e->val.bin.op);
    if (!op) {
      ULASPANIC("Binary operator was NULL\n");
    }
    int left = ulas_intexpreval((int)e->val.bin.left, rc);
    int right = ulas_intexpreval((int)e->val.bin.right, rc);
    switch ((int)op->type) {
    case ULAS_EQ:
      return left == right;
    case ULAS_NEQ:
      return left != right;
    case '<':
      return left < right;
    case '>':
      return left > right;
    case ULAS_LTEQ:
      return left <= right;
    case ULAS_GTEQ:
      return left >= right;
    case '+':
      return left + right;
    case '-':
      return left - right;
    case '*':
      return left * right;
    case '/':
      if (ulas.pass != ULAS_PASS_FINAL) {
        return 0;
      }
      if (right == 0) {
        ULASERR("integer division by 0\n");
        *rc = -1;
        return 0;
      }
      return left / right;
    case '%':
      if (ulas.pass != ULAS_PASS_FINAL) {
        return 0;
      }
      if (right == 0) {
        ULASERR("integer division by 0\n");
        *rc = -1;
        return 0;
      }
      return left % right;
    case ULAS_RSHIFT:
      return left >> right;
    case ULAS_LSHIFT:
      return left << right;
    case '|':
      return left | right;
    case '&':
      return left & right;
    case '^':
      return left ^ right;
    default:
      ULASPANIC("Unhandeled binary operator\n");
      break;
    }
    break;
  }
  case ULAS_EXPUN: {
    struct ulas_tok *op = ulas_tokbufget(&ulas.toks, (int)e->val.un.op);
    if (!op) {
      ULASPANIC("Unary operator was NULL\n");
    }
    int right = ulas_intexpreval((int)e->val.un.right, rc);
    switch ((int)op->type) {
    case '!':
      return !right;
    case '-':
      return -right;
    case '+':
      return +right;
    case '~':
      return ~right;
    default:
      ULASPANIC("Unhandeled unary operation\n");
      break;
    }
    break;
  }
  case ULAS_EXPGRP: {
    return ulas_intexpreval((int)e->val.grp.head, rc);
  }
  case ULAS_EXPPRIM: {
    struct ulas_tok *t = ulas_tokbufget(&ulas.toks, (int)e->val.prim.tok);
    return ulas_valint(t, rc);
  }
  }

  return 0;
}

int ulas_intexpr(const char **line, unsigned long n, int *rc) {
  if (ulas_tokexpr(line, n) == -1) {
    *rc = -1;
    return -1;
  }

  int expr = ulas_parseexpr();
  if (expr == -1) {
    *rc = -1;
    return -1;
  }

  // execute the tree of expressions
  return ulas_intexpreval(expr, rc);
}

char *ulas_strexpr(const char **line, unsigned long n, int *rc) {
  if (ulas_tokexpr(line, n) == -1) {
    *rc = -1;
    return NULL;
  }

  int expr = ulas_parseexpr();
  if (expr == -1) {
    *rc = -1;
    return NULL;
  }

  struct ulas_expr *e = ulas_exprbufget(&ulas.exprs, expr);
  if (!e) {
    ULASERR("unable to evaluate expression\n");
    *rc = -1;
    return NULL;
  }

  switch ((int)e->type) {
  case ULAS_EXPPRIM: {
    struct ulas_tok *t = ulas_tokbufget(&ulas.toks, (int)e->val.prim.tok);
    char *s = ulas_valstr(t, rc);
    return s;
  }
  default:
    ULASERR("Unhandeled string expression\n");
    *rc = -1;
    return NULL;
  }
  return NULL;
}

const char *ulas_asmregstr(unsigned int reg) {
  if (reg >= ulas.arch.regs_len) {
    return NULL;
  }

  return ulas.arch.regs_names[reg];
}

#define ULAS_INSTRBUF_MIN 4
int ulas_asminstr(char *dst, unsigned long max, const char **line,
                  unsigned long n) {
  const char *start = *line;
  if (max < ULAS_INSTRBUF_MIN) {
    ULASPANIC("Instruction buffer is too small!");
    return -1;
  }

  const struct ulas_instr *instrs = ulas.arch.instrs;

  int written = 0;
  while (instrs->name && written == 0) {
    *line = start;
    if (ulas_tok(&ulas.tok, line, n) == -1) {
      ULASERR("Expected instruction\n");
      return -1;
    }

    // check for instruction name first
    if (strncmp(ulas.tok.buf, instrs->name, ulas.tok.maxlen) != 0) {
      goto skip;
    }

    // expression results in order they appear
    // TODO: this should probably become a union of sort to allow float
    // expressions
    int exprres[ULAS_INSTRDATMAX];
    int expridx = 0;
    memset(&exprres, 0, sizeof(int) * ULAS_INSTRDATMAX);

    // then check for each single token...
    const short *tok = instrs->tokens;
    int i = 0;
    while (tok[i]) {
      assert(i < ULAS_INSTRTOKMAX);
      const char *regstr = ulas_asmregstr(tok[i]);
      if (regstr) {
        if (ulas_tok(&ulas.tok, line, n) == -1) {
          goto skip;
        }

        if (strncmp(regstr, ulas.tok.buf, ulas.tok.maxlen) != 0) {
          goto skip;
        }
      } else if (tok[i] == ULAS_E8 || tok[i] == ULAS_E16 || tok[i] == ULAS_A8 ||
                 tok[i] == ULAS_A16) {
        assert(expridx < ULAS_INSTRDATMAX);
        int rc = 0;
        int res = ulas_intexpr(line, n, &rc);
        exprres[expridx++] = res;
        if (rc == -1) {
          return -1;
        }

        if (ULASWARNLEVEL(ULAS_WARN_OVERFLOW) && (unsigned int)res > 0xFF &&
            tok[i] == ULAS_E8) {
          ULASWARN(
              "Warning: 0x%X overflows the maximum allowed value of 0xFF\n",
              res);
        } else if (ULASWARNLEVEL(ULAS_WARN_OVERFLOW) &&
                   (unsigned int)res > 0xFFFF && tok[i] == ULAS_E16) {
          ULASWARN(
              "Warning: 0x%X overflows the maximum allowed value of 0xFFFF\n",
              res);
        }
      } else {
        if (ulas_tok(&ulas.tok, line, n) == -1) {
          goto skip;
        }

        char c[2] = {(char)tok[i], '\0'};
        if (strncmp(ulas.tok.buf, c, ulas.tok.maxlen) != 0) {
          goto skip;
        }
      }

      i++;
    }

    // we are good to go!
    int datread = 0;
    expridx = 0;
    const short *dat = instrs->data;
    while (dat[datread]) {
      assert(datread < ULAS_INSTRDATMAX);
      assert(expridx < ULAS_INSTRDATMAX);

      if (dat[datread] == ULAS_E8 || dat[datread] == ULAS_A8) {
        dst[written] = (char)exprres[expridx++];
      } else if (dat[datread] == ULAS_E16 || dat[datread] == ULAS_A16) {
        short val = (short)exprres[expridx++];
        if (ulas.arch.endianess == ULAS_BE) {
          dst[written++] = (char)(val >> 8);
          dst[written] = (char)(val & 0xFF);
        } else {
          // write 16-bit le values
          dst[written++] = (char)(val & 0xFF);
          dst[written] = (char)(val >> 8);
        }
      } else {
        dst[written] = (char)dat[datread];
      }
      written++;
      datread++;
    }

  skip:
    instrs++;
  }

  if (!written) {
    ulas_trimend('\n', (char *)start, n);
    ULASERR("Invalid instruction: %s\n", start);
    return -1;
  }

  return written;
}

void ulas_asmlst(const char *line, const char *outbuf, unsigned long n) {
  // only write to dst on final pass
  if (ulaslstout && ulas.pass == ULAS_PASS_FINAL) {
    fprintf(ulaslstout, "%08X", ulas.address);

    // always pad at least n bytes
    fputs("  ", ulaslstout);
    const int pad = 10;
    int outwrt = 0;

    for (long i = 0; i < n; i++) {
      outwrt += fprintf(ulaslstout, "%02x ", outbuf[i] & 0xFF);
    }

    for (long i = outwrt; i < pad; i++) {
      fputs(".", ulaslstout);
    }

    fprintf(ulaslstout, "  (%s:%04ld) %s", ulas.filename, ulas.line, line);
  }
}

void ulas_asmout(FILE *dst, const char *outbuf, unsigned long n) {
  // only write to dst on final pass
  if (ulas.pass == ULAS_PASS_FINAL) {
    fwrite(outbuf, 1, n, dst);
  }

  if (ulas.address < 0x14C) {
    for (int i = 0; i < n; i++) {
      ulas.chksm = (char)(ulas.chksm - outbuf[i] - 1);
    }
  }
}

int ulas_asmdirbyte(FILE *dst, const char **line, unsigned long n, int *rc) {
  // .db expr, expr, expr
  struct ulas_tok t;
  int written = 0;
  memset(&t, 0, sizeof(t));

  do {
    int val = ulas_intexpr(line, n, rc);
    char w = (char)val;
    ulas_asmout(dst, &w, 1);

    written++;
    if (ulas_tok(&ulas.tok, line, n) > 0) {
      t = ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), rc);
    } else {
      break;
    }
  } while (*rc != -1 && t.type == ',');

  // go back one byte if the token was a comment
  if (t.type == ULAS_TOK_COMMENT) {
    *line = *line - strlen(ulas.tok.buf);
  }

  return written;
}

int ulas_asmdirset(const char **line, unsigned long n, enum ulas_type t) {
  char name[ULAS_SYMNAMEMAX];
  ulas_tok(&ulas.tok, line, n);
  if (!ulas_isname(ulas.tok.buf, ulas.tok.maxlen)) {
    ULASERR("Unexpected token '%s'\n", ulas.tok.buf);
    return -1;
  }
  strncpy(name, ulas.tok.buf, ULAS_SYMNAMEMAX);

  // consume =
  ulas_tok(&ulas.tok, line, n);
  if (strncmp(ulas.tok.buf, "=", ulas.tok.maxlen) != 0) {
    ULASERR("Unexpected token '%s'. Expected '='\n", ulas.tok.buf);
    return -1;
  }

  int rc = 0;

  union ulas_val val = {0};
  switch (t) {
  case ULAS_INT:
    val.intv = ulas_intexpr(line, n, &rc);
    if (rc == -1) {
      goto fail;
    }
    break;
  case ULAS_STR:
    val.strv = ulas_strexpr(line, n, &rc);
    if (rc == -1) {
      goto fail;
    }
    break;
  default:
    ULASERR("Unexpected type\n");
    return -1;
  }
  struct ulas_tok tok = {t, val};

  if (ulas.pass == ULAS_PASS_FINAL) {
    // only really define in final pass
    ulas_symbolset(name, -1, tok, 0);
  }
fail:
  return rc;
}

int ulas_asmdirset_lookup(const char **line, unsigned long n) {
  if (ulas.pass != ULAS_PASS_FINAL) {
    *line += strlen(*line);
    return 0;
  }
  const char *start = *line;
  ulas_tok(&ulas.tok, line, n);

  int rc = 0;
  struct ulas_sym *found = ulas_symbolresolve(ulas.tok.buf, -1, &rc);

  if (rc == -1 || !found) {
    ULASERR("Unable to set symbol '%s'\n", ulas.tok.buf);
    return -1;
  }

  enum ulas_type t = found->tok.type;

  *line = start;

  return ulas_asmdirset(line, n, t);
}

int ulas_asmdirdef(const char **line, unsigned long n) {
  // .set <str,int> = name expr

  ulas_tok(&ulas.tok, line, n);
  enum ulas_type t = ULAS_INT;
  if (strncmp(ulas.tok.buf, "int", ulas.tok.maxlen) == 0) {
    t = ULAS_INT;
  } else if (strncmp(ulas.tok.buf, "str", ulas.tok.maxlen) == 0) {
    t = ULAS_STR;
  } else {
    ULASERR("Type (str,int) expected. Got '%s'\n", ulas.tok.buf);
    return -1;
  }

  return ulas_asmdirset(line, n, t);
}

int ulas_asmdirdefenum(const char **line, unsigned long n) {
  char name[ULAS_SYMNAMEMAX];
  ulas_tok(&ulas.tok, line, n);
  if (!ulas_isname(ulas.tok.buf, ulas.tok.maxlen)) {
    ULASERR("Unexpected token '%s'\n", ulas.tok.buf);
    return -1;
  }
  strncpy(name, ulas.tok.buf, ULAS_SYMNAMEMAX);

  // consume ,
  ulas_tok(&ulas.tok, line, n);
  if (strncmp(ulas.tok.buf, ",", ulas.tok.maxlen) != 0) {
    ULASERR("Unexpected token '%s'. Expected ','\n", ulas.tok.buf);
    return -1;
  }

  union ulas_val val = {0};
  val.intv = ulas.enumv;

  int rc = 0;
  ULAS_EVALEXPRS(ulas.enumv += ulas_intexpr(line, n, &rc));
  if (rc == -1) {
    goto fail;
  }
  struct ulas_tok tok = {ULAS_INT, val};

  // only really define in final pass
  ulas_symbolset(name, -1, tok, 1);
fail:
  return rc;
}

int ulas_asmdirfill(FILE *dst, const char **line, unsigned long n, int *rc) {
  // fill <what>, <how many>
  int written = 0;

  int ival = ulas_intexpr(line, n, rc);
  char val = (char)ival;
  if (*rc == -1) {
    return 0;
  }

  ulas_tok(&ulas.tok, line, n);
  struct ulas_tok t =
      ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), rc);

  if (*rc == -1 || t.type != ',') {
    ULASERR("Expected ,\n");
    return 0;
  }
  int count = 0;

  ULAS_EVALEXPRS(count = ulas_intexpr(line, n, rc));
  if (count < 0) {
    ULASERR("Count must be positive\n");
    return 0;
  }

  if (*rc == -1) {
    return 0;
  }

  for (int i = 0; i < count; i++) {
    ulas_asmout(dst, &val, 1);
    written++;
  }

  return written;
}

int ulas_asmdirstr(FILE *dst, const char **line, unsigned long n, int *rc) {
  // .str expr, expr, expr
  struct ulas_tok t;
  unsigned long written = 0;
  memset(&t, 0, sizeof(t));

  do {
    char *s = ulas_strexpr(line, n, rc);
    if (!s || *rc != 0) {
      *rc = -1;
      return 0;
    }
    unsigned long len = strlen(s);

    // apply char code map
    for (int i = 0; i < len; i++) {
      s[i] = ulas.charcodemap[(int)s[i]];
    }

    ulas_asmout(dst, s, len);

    written += len;
    if (ulas_tok(&ulas.tok, line, n) > 0) {
      t = ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), rc);
    } else {
      break;
    }
  } while (*rc != -1 && t.type == ',');

  return (int)written;
}

int ulas_asmdirincbin(FILE *dst, const char **line, unsigned long n, int *rc) {
  char *path = ulas_strexpr(line, n, rc);
  char buf[256];
  memset(buf, 0, 256);
  unsigned long written = 0;

  FILE *f = ulas_incpathfopen(path, "re");
  if (!f) {
    *rc = -1;
    return 0;
  }

  unsigned long read = 0;
  while ((read = fread(buf, 1, 256, f))) {
    ulas_asmout(dst, buf, read);
    written += read;
  }

  fclose(f);

  return (int)written;
}

int ulas_asmdiradv(FILE *dst, const char **line, unsigned long n, int *rc) {
  ULAS_EVALEXPRS(ulas.address += ulas_intexpr(line, strnlen(*line, n), rc));
  return 0;
}

int ulas_asmdirsetenum(FILE *dst, const char **line, unsigned long n, int *rc) {
  ULAS_EVALEXPRS(ulas.enumv = ulas_intexpr(line, strnlen(*line, n), rc));
  return 0;
}

int ulas_asmdirsetcharcode(const char **line, unsigned long n) {
  int rc = 0;
  int charcode = 0;
  int setto = 0;
  ULAS_EVALEXPRS(charcode = ulas_intexpr(line, n, &rc));
  charcode = charcode & 0xFF;

  ulas_tok(&ulas.tok, line, n);
  struct ulas_tok t =
      ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), &rc);

  if (rc == -1 || t.type != '=') {
    ULASERR("Expected =\n");
    return 0;
  }

  ULAS_EVALEXPRS(setto = ulas_intexpr(line, n, &rc));
  setto = setto & 0xFF;

  ulas.charcodemap[charcode] = (char)setto;

  return rc;
}

int ulas_asmdirchr(FILE *dst, const char **line, unsigned long n, int *rc) {
  unsigned char b[2] = {0, 0};
  struct ulas_tok t;
  memset(&t, 0, sizeof(t));
  int bit = 7;
  ulas_tok(&ulas.tok, line, n);

  unsigned long len = strlen(ulas.tok.buf);
  if (len > 8) {
    *rc = -1;
    ULASERR("chr input exceeds 8 pixels\n");
    return 0;
  }

  for (int i = 0; i < len; i++, bit--) {
    switch (ulas.tok.buf[i]) {
    case '0':
      // 0 sets no bit
      break;
    case '1':
      // 1 sets 01
      b[1] |= (char)(1 << bit);
      break;
    case '2':
      // 2 sets 10
      b[0] |= (char)(1 << bit);
      break;
    case '3':
      // 3 sets 11
      b[0] |= (char)(1 << bit);
      b[1] |= (char)(1 << bit);
      break;
    default:
      ULASERR("Invalid chr value: '%c'. Expected 0-3.\n", ulas.tok.buf[i]);
      break;
    }
  }

  int written = 2;
  ulas_asmout(dst, (char *)b, written);

  // this always writes 2 bytes
  return written;
}

int ulas_asmdirrep(FILE *dst, FILE *src, const char **line, unsigned long n) {
  char name[ULAS_SYMNAMEMAX];
  ulas_tok(&ulas.tok, line, n);
  if (!ulas_isname(ulas.tok.buf, ulas.tok.maxlen)) {
    ULASERR("Unexpected token '%s'\n", ulas.tok.buf);
    return -1;
  }
  strncpy(name, ulas.tok.buf, ULAS_SYMNAMEMAX);

  // consume ,
  ulas_tok(&ulas.tok, line, n);
  if (strncmp(ulas.tok.buf, ",", ulas.tok.maxlen) != 0) {
    ULASERR("Unexpected token '%s'. Expected ','\n", ulas.tok.buf);
    return -1;
  }

  union ulas_val val = {0};
  val.intv = 0;
  struct ulas_tok tok = {ULAS_INT, val};
  ulas_symbolset(name, ulas.scope, tok, 0);

  int repval = 0;
  int rc = 0;

  ULAS_EVALEXPRS(repval = ulas_intexpr(line, n, &rc));
  ulas_tok(&ulas.tok, line, n);
  struct ulas_tok t =
      ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), &rc);
  if (rc == -1 || t.type != ',') {
    ULASERR("Expected ,\n");
    return 0;
  }

  int step = 0;
  ULAS_EVALEXPRS(step = ulas_intexpr(line, n, &rc));
  ulas_tok(&ulas.tok, line, n);
  t = ulas_totok(ulas.tok.buf, strnlen(ulas.tok.buf, ulas.tok.maxlen), &rc);
  if (rc == -1 || t.type != ',') {
    ULASERR("Expected ,\n");
    return 0;
  }

  unsigned long argline_len = strlen(*line);

  for (int i = 0; i < repval; i += step) {
    tok.val.intv = i;
    ulas_symbolset(name, ulas.scope, tok, 0);
    rc = ulas_asmline(dst, src, *line, argline_len);
    if (rc == -1) {
      break;
    }
  }

  *line += argline_len;
  return rc;
}

int ulas_asmline(FILE *dst, FILE *src, const char *line, unsigned long n) {
  // this buffer is written both to dst and to verbose output
  char outbuf[ULAS_OUTBUFMAX];
  memset(outbuf, 0, ULAS_OUTBUFMAX);
  long towrite = 0;
  long other_writes = 0;

  const char *start = line;
  const char *instr_start = start;
  int rc = 0;

  // read the first token and decide
  ulas_tok(&ulas.tok, &line, n);

  // we ignore empty lines or all comments lines
  if (ulas_istokend(&ulas.tok)) {
    ulas_asmlst(start, outbuf, towrite);
    return 0;
  }

  // is it a label?
  if (ulas_islabelname(ulas.tok.buf, strlen(ulas.tok.buf))) {
    instr_start = line;
    struct ulas_tok label_tok = {ULAS_INT, {(int)ulas.address}};
    if (ulas_symbolset(ulas.tok.buf, -1, label_tok, 1) == -1) {
      return -1;
    }
    ulas_tok(&ulas.tok, &line, n);
    // is next token empty?
    if (ulas_istokend(&ulas.tok)) {
      ulas_asmlst(start, outbuf, towrite);
      return 0;
    }
  }

  if (ulas.tok.buf[0] == ULAS_TOK_ASMDIR_BEGIN) {
    const char *dirstrs[] = {ULAS_ASMSTR_ORG,          ULAS_ASMSTR_SET,
                             ULAS_ASMSTR_BYTE,         ULAS_ASMSTR_STR,
                             ULAS_ASMSTR_FILL,         ULAS_ASMSTR_PAD,
                             ULAS_ASMSTR_INCBIN,       ULAS_ASMSTR_DEF,
                             ULAS_ASMSTR_CHKSM,        ULAS_ASMSTR_ADV,
                             ULAS_ASMSTR_SET_ENUM_DEF, ULAS_ASMSTR_DEFINE_ENUM,
                             ULAS_ASMSTR_SETCHRCODE,   ULAS_ASMSTR_CHR,
                             ULAS_ASMSTR_REP,          NULL};
    enum ulas_asmdir dirs[] = {
        ULAS_ASMDIR_ORG,          ULAS_ASMDIR_SET,
        ULAS_ASMDIR_BYTE,         ULAS_ASMDIR_STR,
        ULAS_ASMDIR_FILL,         ULAS_ASMDIR_PAD,
        ULAS_ASMDIR_INCBIN,       ULAS_ASMDIR_DEF,
        ULAS_ASMDIR_CHKSM,        ULAS_ASMDIR_ADV,
        ULAS_ASMDIR_SET_ENUM_DEF, ULAS_ASMDIR_DEFINE_ENUM,
        ULAS_ASMDIR_SETCHRCODE,   ULAS_ASMDIR_CHR,
        ULAS_ASMDIR_REP};

    enum ulas_asmdir dir = ULAS_ASMDIR_NONE;

    for (long i = 0; dirstrs[i]; i++) {
      if (strncmp(ulas.tok.buf, dirstrs[i], n) == 0) {
        dir = dirs[i];
        break;
      }
    }

    if (!dir) {
      ULASERR("Unexpected directive\n");
      rc = -1;
      goto fail;
    }

    switch (dir) {
    case ULAS_ASMDIR_ORG: {
      ULAS_EVALEXPRS(ulas.address =
                         ulas_intexpr(&line, strnlen(start, n), &rc));
      break;
    }
    case ULAS_ASMDIR_DEF:
      // only do this in the final pass
      rc = ulas_asmdirdef(&line, n);
      break;
    case ULAS_ASMDIR_SET:
      rc = ulas_asmdirset_lookup(&line, n);
      break;
    case ULAS_ASMDIR_BYTE:
      other_writes += ulas_asmdirbyte(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_FILL:
      other_writes += ulas_asmdirfill(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_STR:
      other_writes += ulas_asmdirstr(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_INCBIN:
      other_writes += ulas_asmdirincbin(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_CHKSM:
      ulas_asmout(dst, &ulas.chksm, 1);
      other_writes += 1;
      break;
    case ULAS_ASMDIR_ADV:
      ulas_asmdiradv(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_SET_ENUM_DEF:
      ulas_asmdirsetenum(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_DEFINE_ENUM:
      rc = ulas_asmdirdefenum(&line, n);
      break;
    case ULAS_ASMDIR_SETCHRCODE:
      rc = ulas_asmdirsetcharcode(&line, n);
      break;
    case ULAS_ASMDIR_CHR:
      other_writes += ulas_asmdirchr(dst, &line, n, &rc);
      break;
    case ULAS_ASMDIR_REP:
      rc = ulas_asmdirrep(dst, src, &line, n);
      break;
    case ULAS_ASMDIR_PAD:
      // TODO: pad is the same as .fill n, $ - n
    case ULAS_ASMDIR_NONE:
      ULASPANIC("asmdir not implemented\n");
      break;
    }

  } else {
    // is regular line in form of [label:] instruction ; comment
    // start over for the next step...
    line = instr_start;

    int nextwrite = ulas_asminstr(outbuf, ULAS_OUTBUFMAX, &line, n);
    if (nextwrite == -1) {
      ULASERR("Unable to assemble instruction\n");
      rc = -1;
      goto fail;
    }
    towrite += nextwrite;
  }

  // check for trailing
  // but only if its not a comment
  if (ulas_tok(&ulas.tok, &line, n) > 0) {
    if (!ulas_istokend(&ulas.tok)) {
      ULASERR("Trailing token '%s'\n", ulas.tok.buf);
      return -1;
    }
  }

  ulas_asmout(dst, outbuf, towrite);
  ulas_asmlst(start, outbuf, towrite);

  ulas.address += towrite;
  ulas.address += other_writes;

fail:
  return rc;
}

int ulas_asmnext(FILE *dst, FILE *src, char *buf, int n) {
  int rc = 1;
  if (fgets(buf, n, src) != NULL) {
    unsigned long buflen = strlen(buf);
    if (ulas_asmline(dst, src, buf, buflen) == -1) {
      rc = -1;
    }
  } else {
    rc = 0;
  }
  return rc;
}

int ulas_asm(FILE *dst, FILE *src) {
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);
  int rc = 0;

  // preproc
  while ((rc = ulas_asmnext(dst, src, buf, ULAS_LINEMAX)) > 0) {
  }

  return rc;
}
