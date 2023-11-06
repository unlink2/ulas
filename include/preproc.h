#ifndef PREPROC_H_
#define PREPROC_H_

#include <stdbool.h>
#include <stdio.h>

/*
 * Preprocessor
 */

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
int ulas_preproc(FILE *dst, const char *dstname, FILE *src, const char *srcname);

#endif
