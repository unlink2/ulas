#ifndef PREPROC_H_
#define PREPROC_H_

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
};

/**
 * Tokenize and apply the preprocessor
 */
int ulas_preproc(FILE *dst, FILE *src);

#endif
