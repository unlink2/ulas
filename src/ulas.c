#include "ulas.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

FILE *ulasin = NULL;
FILE *ulasout = NULL;
FILE *ulaserr = NULL;
struct ulas_config ulascfg;
struct ulas ulas;

void ulas_init(struct ulas_config cfg) {
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

  memset(&ulas, 0, sizeof(ulas));
}

int ulas_icntr(void) { return ulas.icntr++; }

struct ulas_config ulas_cfg_from_env(void) {
  struct ulas_config cfg;
  memset(&cfg, 0, sizeof(cfg));

  return cfg;
}

int ulas_main(struct ulas_config cfg) {
  int rc = 0;
  if (cfg.output_path) {
    ulasout = fopen(cfg.output_path, "we");
    if (!ulasout) {
      fprintf(ulaserr, "%s: %s\n", cfg.output_path, strerror(errno));
      free(cfg.output_path);
      return -1;
    }
  }

  if (cfg.argc > 0) {
    ulasin = fopen(cfg.argv[0], "re");
    if (!ulasin) {
      fprintf(ulaserr, "%s: %s\n", cfg.argv[0], strerror(errno));
      return -1;
    }
  }

  ulas_init(cfg);

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

  // TODO: rest of steps here

cleanup:
  if (!cfg.preproc_only) {
    fclose(preprocdst);
  }

  if (cfg.output_path) {
    fclose(ulasout);
    free(cfg.output_path);
  }

  if (cfg.argc > 0) {
    fclose(ulasin);
  }

  return rc;
}

int ulas_isname(const char *tok, size_t n) {
  if (n == 0) {
    return 0;
  }

  if (isdigit(tok[0])) {
    return 0;
  }

  for (size_t i = 0; i < n; i++) {
    char c = tok[i];
    if (c != '_' && !isalnum(c)) {
      return 0;
    }
  }

  return 1;
}

#define WELD_TOKISTERM write
#define WELD_TOKCOND (i < n && write < n && line[i])

int ulas_tok(struct ulas_str *dst, const char **out_line, size_t n) {
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
    case ',':
    case '+':
    case '-':
    case '*':
    case '/':
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
                  size_t n) {
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

#undef WELD_TOKCOND
#undef WLED_TOKISTERM

struct ulas_str ulas_str(size_t n) {
  struct ulas_str str = {malloc(n), n};
  return str;
}

struct ulas_str ulas_strensr(struct ulas_str *s, size_t maxlen) {
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

void ulas_strfree(struct ulas_str *s) {
  if (s->buf) {
    free(s->buf);
  }
}

struct ulas_ppdef *ulas_preprocgetdef(struct ulas_preproc *pp, const char *name,
                                      size_t maxlen) {
  for (size_t i = 0; i < pp->defslen; i++) {
    struct ulas_ppdef *def = &pp->defs[i];
    if (!def->undef && strncmp(def->name, name, maxlen) == 0) {
      return def;
    }
  }

  return NULL;
}

// inserts all leading white space from praw_line into linebuf
int ulas_preproclws(struct ulas_preproc *pp, const char *praw_line,
                    size_t maxlen) {
  int i = 0;
  while (i < maxlen && praw_line[i] && isspace(praw_line[i])) {
    i++;
  }

  size_t linelen = strnlen(pp->line.buf, maxlen);
  ulas_strensr(&pp->line, linelen + i + 1);
  strncat(pp->line.buf, praw_line, i);
  return i;
}

void ulas_trimend(char c, char *buf, size_t n) {
  size_t buflen = strnlen(buf, n);
  // remove trailing new line if present
  while (buf[buflen - 1] == '\n') {
    buf[buflen - 1] = '\0';
    buflen--;
  }
}

char *ulas_preprocexpand(struct ulas_preproc *pp, const char *raw_line,
                         size_t *n) {
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
        size_t val_len = strlen(def->value);
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
        size_t linelen = strlen(praw_line);
        // clear all params from previous attempt
        for (size_t i = 0; i < ULAS_MACROPARAMMAX; i++) {
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
        size_t vallen = strlen(def->value);
        size_t valread = 0;

        // the pointer to tocat will be the variable's value if any
        // exists
        const char *tocat = NULL;
        size_t tocatlen = 0;

        // now tokenize the macro's value and look for $0-$9
        // and replace those instances
        // eveyrthing else will just be copied as is
        while ((valread = ulas_tok(&pp->macrobuf, &val, vallen)) > 0) {
          tocat = NULL;
          char numbuf[128];

          // decide what tocat should be
          for (size_t mi = 0; mi < ULAS_MACROPARAMMAX; mi++) {
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
                     const char *raw_line, size_t n) {
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
    for (size_t i = 0; dirstrs[i]; i++) {
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

        size_t len = strnlen(pp->line.buf, pp->line.maxlen);
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

    size_t buflen = strlen(buf);

    rc = ulas_preprocline(pp, dst, src, buf, buflen);
  } else {
    rc = 0;
  }

  return rc;
}

struct ulas_preproc ulas_preprocinit(void) {
  struct ulas_preproc pp = {NULL, 0, ulas_str(1), ulas_str(1)};
  for (size_t i = 0; i < ULAS_MACROPARAMMAX; i++) {
    pp.macroparam[i] = ulas_str(8);
  }
  pp.macrobuf = ulas_str(8);
  return pp;
}

void ulas_preprocfree(struct ulas_preproc *pp) {
  ulas_strfree(&pp->line);
  ulas_strfree(&pp->tok);

  for (size_t i = 0; i < pp->defslen; i++) {
    if (pp->defs[i].name) {
      free(pp->defs[i].name);
    }
    if (pp->defs[i].value) {
      free(pp->defs[i].value);
    }
  }

  for (size_t i = 0; i < ULAS_MACROPARAMMAX; i++) {
    ulas_strfree(&pp->macroparam[i]);
  }
  ulas_strfree(&pp->macrobuf);

  if (pp->defs) {
    free(pp->defs);
  }
}

int ulas_preproc(FILE *dst, FILE *src) {
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);
  int rc = 0;

  // init
  struct ulas_preproc pp = ulas_preprocinit();

  // preproc
  while ((rc = ulas_preprocnext(&pp, dst, src, buf, ULAS_LINEMAX)) > 0) {
  }

  // cleanup
  ulas_preprocfree(&pp);

  return rc;
}
