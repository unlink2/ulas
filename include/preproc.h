#ifndef PREPROC_H_
#define PREPROC_H_

#include <stdbool.h>
#include <stdio.h>

/*
 * Preprocessor
 */

enum ulas_ppdirs {
  ULAS_PPDIR_NONE = 0,
  ULAS_PPDIR_DEF,
  ULAS_PPDIR_MACRO,
  ULAS_PPDIR_ENDMACRO,
  ULAS_PPDIR_IFDEF,
  ULAS_PPDIR_IFNDEF,
  ULAS_PPDIR_ENDIF
};

enum ulas_ppdefs {
  ULAS_PP_DEF,
  ULAS_PP_MACRO,
};

struct ulas_ppdef {
  enum ulas_ppdefs type;
  bool undef;
};

struct ulas_preproc {
  struct ulas_ppdef *defs;
  size_t defslen;

  const char *srcname;
  const char *dstname;
};

/**
 * Tokenize and apply the preprocessor
 */
int ulas_preproc(FILE *dst, const char *dstname, FILE *src,
                 const char *srcname);

// expand preproc into dst line 
char *ulas_preprocexpand(char *line, size_t linemax, const char *raw_line, size_t *n);

#endif
