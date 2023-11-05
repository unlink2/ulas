#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define ULAS_PATHMAX 4096

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
// start of as directives such as .org
#define ULAS_TOK_ASDIR_BEGIN '.'
// start of preprocessor directives such as #define or #include
#define ULAS_TOK_PREPROC_BEGIN '#'

// format macros
#define ULAS_FMT(f, fmt)                                                       \
  if (isatty(fileno(f)) && ulascfg.color) {                                    \
    fprintf((f), "%s", (fmt));                                                 \
  }

extern FILE *ulasin;
extern FILE *ulasout;
extern FILE *ulaserr;

struct ulas_config {
  // argv represents file names
  char **argv;
  int argc;

  char *output_path;

  bool verbose;
};

/**
 * Tokens
 */

enum ulas_toks {
  ULAS_TOKLITERAL,
  ULAS_TOKINT,
  ULAS_TOKFLOAT,
  ULAS_TOKCHAR,
  ULAS_TOKSTRING
};

struct ulas_tokliteral {
  const char *literal;
};

union ulas_tokdat {
  struct ulas_tokliteral literal;
};

struct ulas_tok {
  enum ulas_toks type;
  union ulas_tokdat dat;
};

/**
 * Symbols
 */

struct ulas_sym {
  const char *name;
};

/**
 * Expressions
 */

struct ulas_expr;

enum ulas_exprs { ULAS_EXPUNARY, ULAS_EXPBINARY, ULAS_EXPLITERAL };

struct ulas_expunary {
  struct ulas_expr *left;
  struct ulas_tok *op;
};

struct ulas_expbinary {
  struct ulas_expr *left;
  struct ulas_expr *right;
  struct ulas_tok *op;
};

struct ulas_expliteral {
  struct ulas_tok *tok;
};

union ulas_expdat {
  struct ulas_expunary unary;
  struct ulas_expbinary binary;
  struct ulas_expliteral literal;
};

struct ulas_expr {
  enum ulas_exprs type;
  union ulas_expdat dat;
};

extern struct ulas_config ulascfg;

struct ulas_config ulas_cfg_from_env(void);
void ulas_init(struct ulas_config cfg);

int ulas_main(struct ulas_config cfg);

char *ulas_strndup(const char *src, size_t n);

/**
 * A token rule returns true when a token should end
 * otherwise returns false
 */
typedef bool (*ulas_tokrule)(char current);

// simple tokenizer at any space char
bool ulas_tokrulespace(char current);

// tokenisze according to pre-defined rules
// returns the amount of bytes of line that were
// consumed or -1 on error
// returns 0 when no more tokens can be read
int ulas_tok(char *dst, const char *line, size_t n, ulas_tokrule rule);

#endif
