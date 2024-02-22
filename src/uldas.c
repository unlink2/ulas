#include "uldas.h"

// dasm the next instruction
// if there are no more bytes to be read, return 0
// on error return -1
// otherwise return 1
int ulas_dasm_next(FILE *src, FILE *dst) {
  unsigned long srctell = ftell(src);
  // read n bytes (as many as in instruction)
  char outbuf[ULAS_OUTBUFMAX];
  memset(outbuf, 0, ULAS_OUTBUFMAX);

  return 0;
}

int ulas_dasm(FILE *src, FILE *dst) {
  // pass 1: run and collect labels
  // pass 2: run and output to file

  int rc = 0;
  while ((rc = ulas_dasm_next(src, dst)) > 0) {
    if (rc == -1) {
      return -1;
    }
  }

  return rc;
}
