#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// Config variables
#define ULAS_CFG_FMT_GREEN "\x1B[32m"
#define ULAS_CFG_FMT_YELLOW "\x1B[33m"
#define ULAS_CFG_FMT_RED "\x1B[31m"
#define ULAS_CFG_FMT_MAGENTA "\x1B[35m"
#define ULAS_CFG_FMT_CYAN "\x1B[36m"
#define ULAS_CFG_FMT_BLUE "\x1B[34m"
#define ULAS_CFG_FMT_RESET "\x1B[0m"

// configurable tokens
#define ULAS_TOK_COMMENT ';'
#define ULAS_TOK_DIRECTIVE_BEGIN '#'

// format macros
#define ULAS_FMT(f, fmt)                                                       \
  if (isatty(fileno(f)) && ulascfg.color) {                                    \
    fprintf((f), "%s", (fmt));                                                 \
  }

extern FILE *ulasin;
extern FILE *ulasout;
extern FILE *ulaserr;

struct ulas_config {
  char **argv;
  int argc;

  bool verbose;
};

extern struct ulas_config ulascfg;

struct ulas_config ulas_cfg_from_env(void);
void ulas_init(struct ulas_config cfg);

int ulas_main(struct ulas_config cfg);

#endif
