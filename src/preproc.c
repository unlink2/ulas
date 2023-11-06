#include "preproc.h"
#include "ulas.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

char *ulas_preprocexpand(char *line, size_t linemax, const char *raw_line,
                         size_t *n) {
  assert(*n <= linemax);
  const char *praw_line = raw_line;

  char tok[ULAS_TOKMAX];

  // go through all tokens, see if a define matches the token,
  // if so expand it
  // only expand macros if they match toks[0] though!
  // otherwise memcpy the read bytes 1:1 into the new string
  while (ulas_tokline(tok, &praw_line, ULAS_TOKMAX, isalnum)) {
  }

  // TODO: actually expand here...
  strncpy(line, raw_line, *n);
  *n = strlen(line);
  return line;
}

int ulas_preprocline(struct ulas_preproc *pp, FILE *dst, const char *raw_line,
                     size_t n) {
  if (n > ULAS_LINEMAX) {
    ULASERR("%s: line exceeds %d (LINEMAX)\n", raw_line, ULAS_LINEMAX);
    return -1;
  }
  assert(n <= ULAS_LINEMAX);
  char line[ULAS_LINEMAX];
  const char *pline = line;

  ulas_preprocexpand(line, ULAS_LINEMAX, raw_line, &n);
  const char *dirstrs[] = {"#define", "#macro",    "#ifdef", "#ifndef",
                           "#endif",  "#endmacro", NULL};
  enum ulas_ppdirs dirs[] = {ULAS_PPDIR_DEF,   ULAS_PPDIR_MACRO,
                             ULAS_PPDIR_IFDEF, ULAS_PPDIR_IFNDEF,
                             ULAS_PPDIR_ENDIF, ULAS_PPDIR_ENDMACRO};

  enum ulas_ppdirs found_dir = ULAS_PPDIR_NONE;

  char tok[ULAS_TOKMAX];

  // check if the first token is any of the valid preproc directives
  if (ulas_tokline(tok, &pline, ULAS_TOKMAX, isspace)) {
    // not a preproc directive...
    if (tok[0] != ULAS_TOK_PREPROC_BEGIN) {
      goto found;
    }
    for (size_t i = 0; dirstrs[i]; i++) {
      if (strncmp(dirstrs[i], tok, ULAS_TOKMAX) == 0) {
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
    printf("%s preproc directive!\n", tok);
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

  while (fgets(buf, ULAS_LINEMAX, src) != NULL) {
    if (ulas_preprocline(&pp, dst, buf, strlen(buf)) == -1) {
      rc = -1;
      goto fail;
    }
  }

fail:
  return rc;
}
