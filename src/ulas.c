#include "ulas.h"
#include <errno.h>
#include <string.h>

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

char *ulas_strndup(const char *src, size_t n) {
  size_t len = MIN(strlen(src), n);
  char *dst = malloc(len);
  strncpy(dst, src, len);
  return dst;
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

int ulas_preproc(FILE *dst, FILE *src) {}
