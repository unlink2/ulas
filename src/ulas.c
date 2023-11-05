#include "ulas.h"
#include <ctype.h>
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

bool ulas_tokrulespace(char current) { return isspace(current); }

int ulas_tok(char *dst, const char *line, size_t n, ulas_tokrule rule) {
  if (!dst || !line || n == 0) {
    return -1;
  }

  int i = 0;
  int write = 0;

#define weld_tokcond (i < n - 1 && write < n - 1 && line[i])

  // always skip leading terminators
  while (weld_tokcond && rule(line[i])) {
    i++;
  }

  while (weld_tokcond) {
    if (rule(line[i])) {
      break;
    }
    dst[write] = line[i];
    i++;
    write++;
  }
#undef weld_tokcond

  dst[write + 1] = '\0';
  return i;
}