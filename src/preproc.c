#include "preproc.h"
#include "ulas.h"

int ulas_preproc(FILE *dst, FILE *src) {
  char buf[ULAS_LINEMAX];
  memset(buf, 0, ULAS_LINEMAX);

  if (!dst || !src) {
    ULASERR("[preproc] Unable to read from dst or write to src!\n");
    return -1;
  }

  while (fgets(buf, ULAS_LINEMAX, src) == NULL) {
  }

  return 0;
}
