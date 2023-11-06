#include "preproc.h"
#include "ulas.h"
#include <stdio.h>
#include <assert.h>


int ulas_preprocline(struct ulas_preproc *pp, FILE *dst, const char *line,
                     size_t n) {

  // check if the first token is any of the valid preproc directives

  assert(fwrite(line, 1, n, dst) == n);

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
