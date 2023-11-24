#include "ulas.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
  }

  ulas.toks = ulas_tokbuf();
  ulas.exprs = ulas_exprbuf();
}

void ulas_free(void) {
  ulas_strfree(&ulas.tok);
  ulas_tokbuffree(&ulas.toks);
  ulas_exprbuffree(&ulas.exprs);
}

int ulas_icntr(void) { return ulas.icntr++; }

struct ulas_config ulas_cfg_from_env(void) {
  struct ulas_config cfg;
  memset(&cfg, 0, sizeof(cfg));

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

int ulas_main(struct ulas_config cfg) {
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

  if (cfg.preproc_only) {
    preprocdst = ulasout;
  } else {
    preprocdst = tmpfile();
  }

  if (ulas_preproc(preprocdst, ulasin) == -1) {
    rc = -1;
    goto cleanup;
  }

  if (cfg.preproc_only) {
    goto cleanup;
  }

cleanup:
  if (!cfg.preproc_only) {
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

  return rc;
}

int ulas_isname(const char *tok, unsigned long n) {
  if (n == 0) {
    return 0;
  }

  if (isdigit(tok[0])) {
    return 0;
  }

  for (unsigned long i = 0; i < n; i++) {
    char c = tok[i];
    if (c != '_' && !isalnum(c)) {
      return 0;
    }
  }

  return 1;
}

struct ulas_tok *ulas_symbolresolve(const char *name) {
  // TODO: implement
  return NULL;
}

#define WELD_TOKISTERM write
#define WELD_TOKCOND (i < n && write < n && line[i])

int ulas_tok(struct ulas_str *dst, const char **out_line, unsigned long n) {
  const char *line = *out_line;
  ulas_strensr(dst, n + 1);

  int i = 0;
  int write = 0;

  // always skip leading terminators
  while (WELD_TOKCOND && isspace(line[i])) {
    i++;
  }

  while (WELD_TOKCOND) {
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
      if (WELD_TOKISTERM) {
        goto tokdone;
      }
      // single char tokens
      dst->buf[write++] = line[i++];
      goto tokdone;
    case '$':
      if (WELD_TOKISTERM) {
        goto tokdone;
      }
      // special var for preprocessor
      // make sure we have enough space in buffer
      ulas_strensr(dst, write + 2);
      // escape char tokens
      dst->buf[write++] = line[i++];
      dst->buf[write++] = line[i++];
      goto tokdone;
    case '=':
    case '<':
    case '!':
    case '>':
      if (line[i + 1] == '=') {
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

  while (WELD_TOKCOND && isspace(line[i])) {
    i++;
  }

  while (WELD_TOKCOND && line[i] != c) {
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
      }
      break;
    case '>':
      if (*buf == '=') {
        tok.type = ULAS_GTEQ;
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

#undef WELD_TOKCOND
#undef WLED_TOKISTERM

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

  // go through all tokens, see if a define matches the token,
  // if so expand it
  // only expand macros if they match toks[0] though!
  // otherwise memcpy the read bytes 1:1 into the new string
  while ((read = ulas_tok(&pp->tok, &praw_line, *n))) {
    // if it is the first token, and it begins with a # do not process at all!
    if (first_tok && pp->tok.buf[0] == ULAS_TOK_PREPROC_BEGIN) {
      ulas_strensr(&pp->line, (*n) + 1);
      strncat(pp->line.buf, raw_line, *n);
      break;
    }
    first_tok = 0;

    struct ulas_ppdef *def =
        ulas_preprocgetdef(pp, pp->tok.buf, pp->tok.maxlen);
    if (def) {
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
               ulas_tokuntil(&pp->macroparam[paramc], ',', &praw_line, *n) >
                   0) {
          // trim new lines from the end of macro params
          ulas_trimend('\n', pp->macroparam[paramc].buf,
                       strlen(pp->macroparam[paramc].buf));
          paramc++;
        }
        ulas_strensr(&pp->line, strlen(def->value) + 2);

        const char *macro_argname[ULAS_MACROPARAMMAX] = {
            "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9"};

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
            ulas_strensr(&pp->line, valread + 1);
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

  const char *dirstrs[] = {ULAS_PPSTR_DEF,   ULAS_PPSTR_MACRO,
                           ULAS_PPSTR_IFDEF, ULAS_PPSTR_IFNDEF,
                           ULAS_PPSTR_ENDIF, ULAS_PPSTR_ENDMACRO,
                           ULAS_PPSTR_UNDEF, NULL};
  enum ulas_ppdirs dirs[] = {ULAS_PPDIR_DEF,   ULAS_PPDIR_MACRO,
                             ULAS_PPDIR_IFDEF, ULAS_PPDIR_IFNDEF,
                             ULAS_PPDIR_ENDIF, ULAS_PPDIR_ENDMACRO,
                             ULAS_PPDIR_UNDEF};

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

void ulas_preprocfree(struct ulas_preproc *pp) {
  ulas_strfree(&pp->line);
  ulas_strfree(&pp->tok);

  for (unsigned long i = 0; i < pp->defslen; i++) {
    if (pp->defs[i].name) {
      free(pp->defs[i].name);
    }
    if (pp->defs[i].value) {
      free(pp->defs[i].value);
    }
  }

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
  struct ulas_preproc pp = ulas_preprocinit();

  long prevseek = 0;
  // preproc
  while ((rc = ulas_preprocnext(&pp, dst, src, buf, ULAS_LINEMAX)) > 0) {
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
  // cleanup
  ulas_preprocfree(&pp);

  return rc;
}

/**
 * Literals, tokens and expressions
 */

int ulas_valint(struct ulas_tok *lit, int *rc) {
  if (!lit || lit->type != ULAS_INT) {
    ULASERR("Expected int\n");
    *rc = -1;
    return 0;
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
  return tb->len++;
}

void ulas_tokbufclear(struct ulas_tokbuf *tb) {
  for (long i = 0; i < tb->len; i++) {
    struct ulas_tok *t = &tb->buf[i];
    if (t->type == ULAS_SYMBOL || t->type == ULAS_STR) {
      free(t->val.strv);
    }
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
  return eb->len++;
}

void ulas_exprbufclear(struct ulas_exprbuf *eb) { eb->len = 0; }

void ulas_exprbuffree(struct ulas_exprbuf *eb) { free(eb->buf); }

/**
 * Assembly step
 */

int ulas_istokend(struct ulas_str *tok) {
  long len = strnlen(tok->buf, tok->maxlen);
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
    if (tok.type == ',' || tok.type == ']' || ulas_istokend(&ulas.tok)) {
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
  if (!t || (t->type != ULAS_INT && t->type != ULAS_STR &&
             t->type != ULAS_SYMBOL && t->type != '(' && t->type != ')')) {
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

  if (t && (t->type == '!' || t->type == '-')) {
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

int ulas_parsecmp(int *i) {
  int expr = ulas_parseterm(i);
  struct ulas_tok *t = NULL;

  while ((t = ulas_tokbufget(&ulas.toks, *i)) &&
         (t->type == ULAS_LTEQ || t->type == ULAS_GTEQ || t->type == '>' ||
          t->type == '<')) {
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

int ulas_parseexprat(int *i) { return ulas_parseeq(i); }

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
    struct ulas_tok *op = ulas_tokbufget(&ulas.toks, e->val.bin.op);
    if (!op) {
      ULASPANIC("Binary operator was NULL\n");
    }
    int left = ulas_intexpreval(e->val.bin.left, rc);
    int right = ulas_intexpreval(e->val.bin.right, rc);
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
      if (right == 0) {
        ULASERR("integer division by 0\n");
        *rc = -1;
        return 0;
      }
      return left / right;
    case '%':
      if (right == 0) {
        ULASERR("integer division by 0\n");
        *rc = -1;
        return 0;
      }
      return left % right;
    default:
      ULASPANIC("Unhandeled binary operator\n");
      break;
    }
    break;
  }
  case ULAS_EXPUN: {
    struct ulas_tok *op = ulas_tokbufget(&ulas.toks, e->val.un.op);
    if (!op) {
      ULASPANIC("Unary operator was NULL\n");
    }
    int right = ulas_intexpreval(e->val.un.right, rc);
    switch ((int)op->type) {
    case '!':
      return !right;
    case '-':
      return -right;
    default:
      ULASPANIC("Unhandeled unary operation\n");
      break;
    }
    break;
  }
  case ULAS_EXPGRP: {
    return ulas_intexpreval(e->val.grp.head, rc);
  }
  case ULAS_EXPPRIM: {
    struct ulas_tok *t = ulas_tokbufget(&ulas.toks, e->val.prim.tok);
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

  int expr = -1;
  if ((expr = ulas_parseexpr()) == -1) {
    *rc = -1;
    return -1;
  }

  // execute the tree of expressions
  return ulas_intexpreval(expr, rc);
}

const char *ulas_asmregstr(enum ulas_asmregs reg) {
  switch (reg) {
  case ULAS_REG_A:
    return "a";
  case ULAS_REG_B:
    return "b";
  case ULAS_REG_C:
    return "c";
  case ULAS_REG_D:
    return "d";
  case ULAS_REG_E:
    return "e";
  case ULAS_REG_H:
    return "h";
  case ULAS_REG_L:
    return "l";
  case ULAS_REG_HL:
    return "hl";
  case ULAS_REG_DE:
    return "de";
  case ULAS_REG_BC:
    return "bc";
  case ULAS_REG_NOT_ZERO:
    return "nz";
  case ULAS_REG_ZERO:
    return "z";
  case ULAS_REG_NOT_CARRY:
    return "nc";
  case ULAS_REG_CARRY:
    return "c";
  case ULAS_REG_SP:
    return "sp";
  case ULAS_REG_AF:
    return "af";
  }

  return NULL;
}

enum ulas_asmregs ulas_asmstrreg(const char *s, unsigned long n) {
  for (int i = 0; i < 10; i++) {
    const char *rs = ulas_asmregstr(i);
    if (rs && strncmp(rs, s, n) == 0) {
      return i;
    }
  }

  return -1;
}

int ulas_asmregisr8(enum ulas_asmregs reg) {
  return reg != ULAS_REG_BC && reg != ULAS_REG_DE && reg != ULAS_REG_HL;
}

/**
 * Instruction table
 */

// define <name> r8, r8
#define ULAS_INSTR_R8R8(name, base_op, reg_left, reg_right)                    \
  {                                                                            \
    (name), {(reg_left), ',', (reg_right), 0}, { base_op, 0 }                  \
  }

#define ULAS_INSTR_R16R16(name, op, reg_left, reg_right)                       \
  ULAS_INSTR_R8R8(name, op, reg_left, reg_right)

#define ULAS_INSTR_R8R8D(name, base_op, reg_left)                              \
  ULAS_INSTR_R8R8(name, base_op, reg_left, ULAS_REG_B),                        \
      ULAS_INSTR_R8R8(name, base_op + 1, reg_left, ULAS_REG_C),                \
      ULAS_INSTR_R8R8(name, base_op + 2, reg_left, ULAS_REG_D),                \
      ULAS_INSTR_R8R8(name, base_op + 3, reg_left, ULAS_REG_E),                \
      ULAS_INSTR_R8R8(name, base_op + 4, reg_left, ULAS_REG_H),                \
      ULAS_INSTR_R8R8(name, base_op + 5, reg_left, ULAS_REG_L),                \
      {(name), {(reg_left), ',', '[', ULAS_REG_HL, ']', 0}, {base_op + 6, 0}}, \
      ULAS_INSTR_R8R8(name, base_op + 7, reg_left, ULAS_REG_A)

// <name> a, r8
#define ULAS_INSTR_ALUR8D(name, base_op)                                       \
  ULAS_INSTR_R8R8D(name, base_op, ULAS_REG_A)

#define ULAS_INSTR_R8_EXPR8(name, op, reg_left)                                \
  {                                                                            \
    (name), {(reg_left), ',', ULAS_E8, 0}, { (op), ULAS_E8, 0 }                \
  }

// <name> r16, e16
#define ULAS_INSTR_R16E16(name, op, reg_left)                                  \
  {                                                                            \
    (name), {(reg_left), ',', ULAS_E16, 0}, { (op), ULAS_E16, 0 }              \
  }

// <name> reg
#define ULAS_INSTR_REG(name, op, reg)                                          \
  {                                                                            \
    (name), {(reg), 0}, { (op), 0x00 }                                         \
  }

// all instructions
// when name is NULL list ended
const struct ulas_instr ULASINSTRS[] = {
    // control instructions
    {"nop", {0}, {(short)ULAS_DATZERO, 0}},
    {"halt", {0}, {0x76, 0}},
    {"stop", {0}, {0x10, (short)ULAS_DATZERO, 0x00}},
    {"di", {0}, {0xF4, 0x00}},
    {"ei", {0}, {0xFB, 0x00}},

    // misc
    {"daa", {0}, {0x27, 0x00}},
    {"scf", {0}, {0x37, 0x00}},

    {"cpl", {0}, {0x2F, 0x00}},
    {"ccf", {0}, {0x3F, 0x00}},

    // shift / bits
    {"rlca", {0}, {0x07, 0x00}},
    {"rls", {0}, {0x17, 0x00}},
    {"rrca", {0}, {0x0F, 0x00}},
    {"rra", {0}, {0x1F, 0x00}},

    // ld r8, r8
    ULAS_INSTR_R8R8D("ld", 0x40, ULAS_REG_B),
    ULAS_INSTR_R8R8D("ld", 0x48, ULAS_REG_C),
    ULAS_INSTR_R8R8D("ld", 0x50, ULAS_REG_D),
    ULAS_INSTR_R8R8D("ld", 0x58, ULAS_REG_E),
    ULAS_INSTR_R8R8D("ld", 0x60, ULAS_REG_H),
    ULAS_INSTR_R8R8D("ld", 0x68, ULAS_REG_L),
    ULAS_INSTR_R8R8D("ld", 0x78, ULAS_REG_A),

    // ld [r16], a
    {"ld", {'[', ULAS_REG_HL, ']', ',', ULAS_E8, 0}, {0x36, ULAS_E8, 0x00}},
    {"ld", {'[', ULAS_REG_BC, ']', ',', ULAS_REG_A, 0}, {0x02, 0}},
    {"ld", {'[', ULAS_REG_DE, ']', ',', ULAS_REG_A, 0}, {0x12, 0}},
    {"ld", {'[', ULAS_REG_HL, '+', ']', ',', ULAS_REG_A, 0}, {0x22, 0}},
    {"ld", {'[', ULAS_REG_HL, '-', ']', ',', ULAS_REG_A, 0}, {0x32, 0}},

    // ld a, [r16]
    {"ld", {ULAS_REG_A, ',', '[', ULAS_REG_BC, ']', 0}, {0x0A, 0}},
    {"ld", {ULAS_REG_A, ',', '[', ULAS_REG_DE, ']', 0}, {0x1A, 0}},
    {"ld", {ULAS_REG_A, ',', '[', ULAS_REG_HL, '+', ']', 0}, {0x2A, 0}},
    {"ld", {ULAS_REG_A, ',', '[', ULAS_REG_HL, '-', ']', 0}, {0x3A, 0}},

    {"ld", {'[', ULAS_E16, ']', ',', ULAS_REG_SP, 0}, {0x08, ULAS_E16, 0}},

    {"ld", {'[', ULAS_E16, ']', ',', ULAS_REG_A, 0}, {0xEA, ULAS_E16, 0}},
    {"ld", {ULAS_REG_A, ',', '[', ULAS_E16, ']', 0}, {0xFA, ULAS_E16, 0}},

    {"ldh", {'[', ULAS_REG_C, ']', ',', ULAS_REG_A, 0}, {0xE2, 0}},
    {"ldh", {ULAS_REG_A, ',', '[', ULAS_REG_C, ']', 0}, {0xF2, 0}},

    {"ldh", {'[', ULAS_E8, ']', ',', ULAS_REG_A, 0}, {0xE0, ULAS_E8, 0}},
    {"ldh", {ULAS_REG_A, ',', '[', ULAS_E8, ']', 0}, {0xF0, ULAS_E8, 0}},

    // ld r8, e8
    ULAS_INSTR_R8_EXPR8("ld", 0x06, ULAS_REG_B),
    ULAS_INSTR_R8_EXPR8("ld", 0x16, ULAS_REG_D),
    ULAS_INSTR_R8_EXPR8("ld", 0x26, ULAS_REG_H),

    ULAS_INSTR_R8_EXPR8("ld", 0x0E, ULAS_REG_C),
    ULAS_INSTR_R8_EXPR8("ld", 0x1E, ULAS_REG_E),
    ULAS_INSTR_R8_EXPR8("ld", 0x2E, ULAS_REG_L),
    ULAS_INSTR_R8_EXPR8("ld", 0x3E, ULAS_REG_A),

    // ld r16, e16
    ULAS_INSTR_R16E16("ld", 0x01, ULAS_REG_BC),
    ULAS_INSTR_R16E16("ld", 0x11, ULAS_REG_DE),
    ULAS_INSTR_R16E16("ld", 0x21, ULAS_REG_HL),
    ULAS_INSTR_R16E16("ld", 0x31, ULAS_REG_SP),

    // jr
    ULAS_INSTR_R8_EXPR8("jr", 0x20, ULAS_REG_NOT_ZERO),
    ULAS_INSTR_R8_EXPR8("jr", 0x30, ULAS_REG_NOT_CARRY),
    {"jr", {ULAS_E8, 0}, {0x18, ULAS_E8, 0x00}},
    ULAS_INSTR_R8_EXPR8("jr", 0x28, ULAS_REG_ZERO),
    ULAS_INSTR_R8_EXPR8("jr", 0x38, ULAS_REG_CARRY),

    // ret
    ULAS_INSTR_REG("ret", 0xC0, ULAS_REG_NOT_ZERO),
    ULAS_INSTR_REG("ret", 0xD0, ULAS_REG_NOT_CARRY),

    // jp
    ULAS_INSTR_R16E16("jp", 0xC2, ULAS_REG_NOT_ZERO),
    ULAS_INSTR_R16E16("jp", 0xD2, ULAS_REG_NOT_CARRY),
    {"jp", {ULAS_E16, 0}, {0xC3, ULAS_E16, 0x00}},

    // inc/dec
    ULAS_INSTR_REG("inc", 0x03, ULAS_REG_BC),
    ULAS_INSTR_REG("inc", 0x13, ULAS_REG_DE),
    ULAS_INSTR_REG("inc", 0x23, ULAS_REG_HL),
    ULAS_INSTR_REG("inc", 0x33, ULAS_REG_SP),

    ULAS_INSTR_REG("inc", 0x04, ULAS_REG_B),
    ULAS_INSTR_REG("inc", 0x14, ULAS_REG_D),
    ULAS_INSTR_REG("inc", 0x24, ULAS_REG_H),
    {"inc", {'[', ULAS_REG_HL, ']', 0}, {0x34, 0x00}},

    ULAS_INSTR_REG("dec", 0x05, ULAS_REG_B),
    ULAS_INSTR_REG("dec", 0x15, ULAS_REG_D),
    ULAS_INSTR_REG("dec", 0x25, ULAS_REG_H),
    {"dec", {'[', ULAS_REG_HL, ']', 0}, {0x35, 0x00}},

    ULAS_INSTR_REG("dec", 0x0B, ULAS_REG_BC),
    ULAS_INSTR_REG("dec", 0x1B, ULAS_REG_DE),
    ULAS_INSTR_REG("dec", 0x2B, ULAS_REG_HL),
    ULAS_INSTR_REG("dec", 0x3B, ULAS_REG_SP),

    ULAS_INSTR_REG("inc", 0x0C, ULAS_REG_C),
    ULAS_INSTR_REG("inc", 0x1C, ULAS_REG_E),
    ULAS_INSTR_REG("inc", 0x2C, ULAS_REG_L),
    ULAS_INSTR_REG("inc", 0x3C, ULAS_REG_A),

    ULAS_INSTR_REG("dec", 0x0D, ULAS_REG_C),
    ULAS_INSTR_REG("dec", 0x1D, ULAS_REG_E),
    ULAS_INSTR_REG("dec", 0x2D, ULAS_REG_L),
    ULAS_INSTR_REG("dec", 0x3D, ULAS_REG_A),

    // alu r8, r8
    ULAS_INSTR_ALUR8D("add", 0x80),
    ULAS_INSTR_ALUR8D("adc", 0x88),
    ULAS_INSTR_ALUR8D("sub", 0x90),
    ULAS_INSTR_ALUR8D("sbc", 0x98),
    ULAS_INSTR_ALUR8D("and", 0xA0),
    ULAS_INSTR_ALUR8D("xor", 0xA8),
    ULAS_INSTR_ALUR8D("or", 0xB0),
    ULAS_INSTR_ALUR8D("cp", 0xB8),

    // alu r16, r16
    ULAS_INSTR_R16R16("add", 0x09, ULAS_REG_HL, ULAS_REG_BC),
    ULAS_INSTR_R16R16("add", 0x19, ULAS_REG_HL, ULAS_REG_DE),
    ULAS_INSTR_R16R16("add", 0x29, ULAS_REG_HL, ULAS_REG_HL),
    ULAS_INSTR_R16R16("add", 0x39, ULAS_REG_HL, ULAS_REG_SP),

    // pop
    ULAS_INSTR_REG("pop", 0xC1, ULAS_REG_BC),
    ULAS_INSTR_REG("pop", 0xD1, ULAS_REG_DE),
    ULAS_INSTR_REG("pop", 0xE1, ULAS_REG_HL),
    ULAS_INSTR_REG("pop", 0xF1, ULAS_REG_AF),

    {NULL}};

// assembles an instruction, writes bytes into dst
// returns bytes written or -1 on error
int ulas_asminstr(char *dst, unsigned long max, const char **line,
                  unsigned long n) {
  const char *start = *line;
  if (max < 4) {
    ULASPANIC("Instruction buffer is too small!");
    return -1;
  }

  // TODO: check for symbol token here... if so add it
  // and skip to the next token

  const struct ulas_instr *instrs = ULASINSTRS;

  int written = 0;
  while (instrs->name && written == 0) {
    *line = start;
    if (ulas_tok(&ulas.tok, line, n) == -1) {
      ULASERR("Expected label or instruction\n");
      return -1;
    }

    // check for instruction name first
    if (strncmp(ulas.tok.buf, instrs->name, ulas.tok.maxlen) != 0) {
      goto skip;
    }

    // expression results in order they appear
    int exprres[ULAS_INSTRDATMAX];
    int expridx = 0;
    memset(&exprres, 0, sizeof(int) * ULAS_INSTRDATMAX);

    // then check for each single token...
    const short *tok = instrs->tokens;
    int i = 0;
    while (tok[i]) {
      assert(i < ULAS_INSTRTOKMAX);
      const char *regstr = NULL;
      if ((regstr = ulas_asmregstr(tok[i]))) {
        if (ulas_tok(&ulas.tok, line, n) == -1) {
          goto skip;
        }

        if (strncmp(regstr, ulas.tok.buf, ulas.tok.maxlen) != 0) {
          goto skip;
        }
      } else if (tok[i] == ULAS_E8 || tok[i] == ULAS_E16) {
        assert(expridx < ULAS_INSTRDATMAX);
        int rc = 0;
        exprres[expridx++] = ulas_intexpr(line, n, &rc);
        if (rc == -1) {
          return -1;
        }
      } else {
        if (ulas_tok(&ulas.tok, line, n) == -1) {
          goto skip;
        }

        char c[2] = {tok[i], '\0'};
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

      if (dat[datread] == ULAS_E8) {
        dst[written] = (char)exprres[expridx++];
      } else if (dat[datread] == ULAS_E16) {
        // write 16-bit le values
        short val = exprres[expridx++];
        dst[written++] = val & 0xFF;
        dst[written] = val >> 8;
      } else {
        dst[written] = dat[datread];
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

void ulas_asmlst(const char *line, char *outbuf, unsigned long n) {
  if (ulaslstout) {
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
  fwrite(outbuf, 1, n, dst);
}

int ulas_asmline(FILE *dst, FILE *src, const char *line, unsigned long n) {
  // this buffer is written both to dst and to verbose output
  char outbuf[ULAS_OUTBUFMAX];
  memset(outbuf, 0, ULAS_OUTBUFMAX);
  long towrite = 0;

  const char *start = line;
  int rc = 0;

  // read the first token and decide
  ulas_tok(&ulas.tok, &line, n);

  // we ignore empty lines or all comments lines
  if (ulas_istokend(&ulas.tok)) {
    ulas_asmlst(start, outbuf, towrite);
    return 0;
  }

  if (ulas.tok.buf[0] == ULAS_TOK_ASMDIR_BEGIN) {
    const char *dirstrs[] = {
        ULAS_ASMSTR_ORG,  ULAS_ASMSTR_SET, ULAS_ASMSTR_BYTE,   ULAS_ASMSTR_STR,
        ULAS_ASMSTR_FILL, ULAS_ASMSTR_PAD, ULAS_ASMSTR_INCBIN, NULL};
    enum ulas_asmdir dirs[] = {
        ULAS_ASMDIR_ORG,  ULAS_ASMDIR_SET, ULAS_ASMDIR_BYTE,  ULAS_ASMDIR_STR,
        ULAS_ASMDIR_FILL, ULAS_ASMDIR_PAD, ULAS_ASMDIR_INCBIN};

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
    case ULAS_ASMDIR_NONE:
    case ULAS_ASMDIR_ORG:
      ulas.address = ulas_intexpr(&line, strnlen(start, n), &rc);
      break;
    case ULAS_ASMDIR_SET:
    case ULAS_ASMDIR_BYTE:
    case ULAS_ASMDIR_STR:
    case ULAS_ASMDIR_FILL:
    case ULAS_ASMDIR_PAD:
    case ULAS_ASMDIR_INCBIN:
      ULASPANIC("asmdir not implemented\n");
      break;
    }

  } else {
    // start over for the next step...
    line = start;
    // is regular line in form of [label:] instruction ; comment
    if ((towrite += ulas_asminstr(outbuf, ULAS_OUTBUFMAX, &line, n)) == -1) {
      ULASERR("Unable to assemble instruction\n");
      rc = -1;
      goto fail;
    }

    // TODO:
    // place marker when a label was unresolved
    // write down byte location in dst as well as the byte offset in lstout so
    // we can fix them later labels can only be unresolved when they are the
    // only node in an expression!
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
