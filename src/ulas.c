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

struct ulas_config ulas_cfg_from_env(void) {
  struct ulas_config cfg;
  memset(&cfg, 0, sizeof(cfg));

  return cfg;
}

int ulas_main(struct ulas_config cfg) {
  if (cfg.output_path) {
    ulasout = fopen(cfg.output_path, "we");
    if (!ulasout) {
      fprintf(ulaserr, "%s: %s\n", cfg.output_path, strerror(errno));
      free(cfg.output_path);
      return -1;
    }
  }

  ulas_init(cfg);

  if (cfg.output_path) {
    fclose(ulasout);
    free(cfg.output_path);
    return -1;
  }

  return 0;
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

int ulas_tok(struct ulas_str *dst, const char **out_line, size_t n) {
  const char *line = *out_line;
  ulas_strensr(dst, n + 1);

  int i = 0;
  int write = 0;

#define weld_tokcond (i < n && write < n && line[i])

  // always skip leading terminators
  while (weld_tokcond && isspace(line[i])) {
    i++;
  }

  char c = line[i];

  switch (c) {
  case ',':
  case '+':
  case '-':
  case '*':
  case '/':
  case '\\':
  case ULAS_TOK_COMMENT:
    // single char tokens
    dst->buf[write++] = line[i++];
    break;
    // consume rest of the line but do not write anything to tokens
    i = (int)n;
    break;
  default:
    while (weld_tokcond) {
      if (isspace(line[i])) {
        break;
      }
      dst->buf[write] = line[i];
      i++;
      write++;
    }
    break;
  }

#undef weld_tokcond

  dst->buf[write] = '\0';

  *out_line += i;
  return i;
}

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

char *ulas_preprocexpand(struct ulas_preproc *pp, const char *raw_line,
                         size_t *n) {
  const char *praw_line = raw_line;
  memset(pp->line.buf, 0, pp->line.maxlen);

  int read = 0;

  // go through all tokens, see if a define matches the token,
  // if so expand it
  // only expand macros if they match toks[0] though!
  // otherwise memcpy the read bytes 1:1 into the new string
  while ((read = ulas_tok(&pp->tok, &praw_line, *n))) {
    for (size_t i = 0; i < pp->defslen; i++) {
      struct ulas_ppdef *def = &pp->defs[i];
      if (strncmp(def->name, pp->tok.buf, pp->tok.maxlen) != 0) {
        continue;
      }

      // if so... expand now and leave
      switch (def->type) {
      case ULAS_PPDEF:
        // adjust total length
        *n -= strlen(pp->tok.buf);
        size_t val_len = strlen(def->value);
        *n += val_len;
        ulas_strensr(&pp->line, (*n) + 1);
        strncat(pp->line.buf, def->value, val_len);
        break;
      case ULAS_PPMACRO:
        // TODO: Implement
        ULASPANIC("PPMACRO is not implemented!\n");
        break;
      }

      goto found;
    }
    // if not found: copy everythin from prev to the current raw_line point -
    // tok lenght -> this keeps the line in-tact as is
    ulas_strensr(&pp->line, (*n) + 1);
    strncat(pp->line.buf, praw_line - read, read);

    // bit of a hack to allow labels at the end of a compound statement
  found:
    continue;
  }

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
  char *line = ulas_preprocexpand(pp, raw_line, &n);
  const char *pline = line;

  const char *dirstrs[] = {ULAS_PPSTR_DEF,
                           ULAS_PPSTR_MACRO,
                           ULAS_PPSTR_IFDEF,
                           ULAS_PPSTR_IFNDEF,
                           ULAS_PPSTR_ENDIF,
                           ULAS_PPSTR_ENDMACRO,
                           NULL};
  enum ulas_ppdirs dirs[] = {ULAS_PPDIR_DEF,   ULAS_PPDIR_MACRO,
                             ULAS_PPDIR_IFDEF, ULAS_PPDIR_IFNDEF,
                             ULAS_PPDIR_ENDIF, ULAS_PPDIR_ENDMACRO};

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
                               false};
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

      struct ulas_str val = ulas_str(32);

      // TODO: handle lines here

      // we leak the str's buffer into the def now
      // this is ok because we call free for it later anyway
      struct ulas_ppdef def = {ULAS_PPDEF, strdup(pp->tok.buf), strdup(val.buf),
                               false};
      ulas_preprocdef(pp, def);

      goto dirdone;
    }
    case ULAS_PPDIR_ENDMACRO:
      break;
    case ULAS_PPDIR_IFDEF:
    case ULAS_PPDIR_IFNDEF:
    case ULAS_PPDIR_ENDIF:
      // TODO: implement
      ULASPANIC("Preproc directive is not implemented!\n");
      break;
    default:
      // this should not happen!
      break;
    }

    // the end of a directive should have no further tokens!
    if (ulas_tok(&pp->tok, &pline, n) != 0) {
      ULASERR("Stray tokens '%s' at end of preprocessor directive\n",
              pp->tok.buf);
      return -1;
    }
  dirdone:
    return found_dir;
  } else {
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

    // remove trailing new line if present
    if (buf[buflen - 1] == '\n') {
      buf[buflen - 1] = '\0';
    }

    rc = ulas_preprocline(pp, dst, src, buf, buflen);
  } else {
    rc = 0;
  }

  return rc;
}

int ulas_preproc(FILE *dst, FILE *src) {
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);
  int rc = 0;

  struct ulas_preproc pp = {NULL, 0, ulas_str(1), ulas_str(1)};

  while ((rc = ulas_preprocnext(&pp, dst, src, buf, ULAS_LINEMAX)) > 0) {
  }

  ulas_strfree(&pp.line);
  ulas_strfree(&pp.tok);

  for (size_t i = 0; i < pp.defslen; i++) {
    if (pp.defs[i].name) {
      free(pp.defs[i].name);
    }
    if (pp.defs[i].value) {
      free(pp.defs[i].value);
    }
  }
  if (pp.defs) {
    free(pp.defs);
  }

  return rc;
}
