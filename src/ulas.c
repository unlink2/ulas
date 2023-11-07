#include "ulas.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

FILE *ulasin = NULL;
FILE *ulasout = NULL;
FILE *ulaserr = NULL;
struct ulas_config ulascfg;

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

int ulas_tok(struct ulas_str *dst, const char *line, size_t n,
             ulas_tokrule rule) {
  if (!dst->buf || !line || n == 0) {
    return -1;
  }
  ulas_strensr(dst, n + 1);

  int i = 0;
  int write = 0;

#define weld_tokcond (i < n && write < n && line[i])

  // always skip leading terminators
  while (weld_tokcond && rule(line[i])) {
    i++;
  }

  while (weld_tokcond) {
    if (rule(line[i])) {
      break;
    }
    dst->buf[write] = line[i];
    i++;
    write++;
  }
#undef weld_tokcond

  dst->buf[write] = '\0';
  return i;
}

int ulas_tokline(struct ulas_str *dst, const char **line, size_t n,
                 ulas_tokrule rule) {
  int rc = ulas_tok(dst, *line, n, rule);
  if (rc == -1) {
    return -1;
  }
  *line += rc;
  return rc;
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
  ulas_strensr(&pp->line, (*n) + 1);

  // go through all tokens, see if a define matches the token,
  // if so expand it
  // only expand macros if they match toks[0] though!
  // otherwise memcpy the read bytes 1:1 into the new string
  while (ulas_tokline(&pp->tok, &praw_line, ULAS_TOKMAX, isalnum)) {
  }

  // TODO: actually expand here...
  strncpy(pp->line.buf, raw_line, (*n) + 1);
  *n = strlen(pp->line.buf);
  return pp->line.buf;
}

int ulas_preprocline(struct ulas_preproc *pp, FILE *dst, const char *raw_line,
                     size_t n) {
  if (n > ULAS_LINEMAX) {
    ULASERR("%s: line exceeds %d (LINEMAX)\n", raw_line, ULAS_LINEMAX);
    return -1;
  }

  char *line = ulas_preprocexpand(pp, raw_line, &n);
  const char *pline = line;

  const char *dirstrs[] = {"#define", "#macro",    "#ifdef", "#ifndef",
                           "#endif",  "#endmacro", NULL};
  enum ulas_ppdirs dirs[] = {ULAS_PPDIR_DEF,   ULAS_PPDIR_MACRO,
                             ULAS_PPDIR_IFDEF, ULAS_PPDIR_IFNDEF,
                             ULAS_PPDIR_ENDIF, ULAS_PPDIR_ENDMACRO};

  enum ulas_ppdirs found_dir = ULAS_PPDIR_NONE;

  // check if the first token is any of the valid preproc directives
  if (ulas_tokline(&pp->tok, &pline, ULAS_TOKMAX, isspace)) {
    // not a preproc directive...
    if (pp->tok.buf[0] != ULAS_TOK_PREPROC_BEGIN) {
      goto found;
    }
    for (size_t i = 0; dirstrs[i]; i++) {
      if (strncmp(dirstrs[i], pp->tok.buf, ULAS_TOKMAX) == 0) {
        found_dir = dirs[i];
        goto found;
      }
    }

    ULASPANIC("Unknown preprocessor directive: %s\n", line);
    return -1;
  }
found:

  if (found_dir) {
    // TODO: process directive
    printf("%s preproc directive!\n", pp->tok.buf);
    fputc('\0', dst);
  } else {
    assert(fwrite(line, 1, n, dst) == n);
  }

  return 0;
}

int ulas_preproc(FILE *dst, const char *dstname, FILE *src,
                 const char *srcname) {
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);
  int rc = 0;

  if (!dst || !src) {
    ULASERR("[%s] Unable to read from dst or write to src!\n", srcname);
    return -1;
  }

  struct ulas_preproc pp = {NULL, 0, srcname, dstname};

  pp.line = ulas_str(1);
  pp.tok = ulas_str(1);

  while (fgets(buf, ULAS_LINEMAX, src) != NULL) {
    if (ulas_preprocline(&pp, dst, buf, strlen(buf)) == -1) {
      rc = -1;
      goto fail;
    }
  }

fail:
  ulas_strfree(&pp.line);
  ulas_strfree(&pp.tok);
  return rc;
}
