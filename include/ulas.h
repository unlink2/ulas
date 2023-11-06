#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define ULAS_PATHMAX 4096
#define ULAS_LINEMAX 4096
#define ULAS_TOKMAX 64

// max tokens per line...
#define ULAS_TOKSMAX 64

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

#define ULASERR(...) fprintf(ulaserr, __VA_ARGS__);
#define ULASWARN(...) fprintf(ulaserr, __VA_ARGS__);

// format macros
#define ULAS_FMT(f, fmt)                                                       \
  if (isatty(fileno(f)) && ulascfg.color) {                                    \
    fprintf((f), "%s", (fmt));                                                 \
  }

extern FILE *ulasin;
extern FILE *ulasout;
extern FILE *ulaserr;

// NULL for int based lookup
#define INULL (-1)

// expression index in the expression list
typedef int iexpr;

// token index in the token list
typedef int itok;

// string index in the string list
typedef int istr;

struct ulas_expr;
struct ulas_tok;

struct ulas_config {
  // argv represents file names
  char **argv;
  int argc;

  char *output_path;

  bool verbose;
};

/**
 * Assembly context
 */

struct ulas {
  char **strs;
  size_t strslen;
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
  istr literal;
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
  istr name;
};

/**
 * Expressions
 *
 * Expressions use an index based lookup instead of
 * actual pointers to allow easy dumping of the structure to a file
 * as well as realloc calls. As an index the integer based iexpr and itok
 * denote which data type is supposed to be looked up. An iexpr or itok value of
 * -1 denotes a NULL value
 */

enum ulas_exprs { ULAS_EXPUNARY, ULAS_EXPBINARY, ULAS_EXPLITERAL };

struct ulas_expunary {
  iexpr left;
  itok op;
};

struct ulas_expbinary {
  iexpr left;
  iexpr right;
  itok op;
};

struct ulas_expliteral {
  itok tok;
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
typedef int (*ulas_tokrule)(int current);


// tokenisze according to pre-defined rules
// returns the amount of bytes of line that were
// consumed or -1 on error
// returns 0 when no more tokens can be read
int ulas_tok(char *dst, const char *line, size_t n, ulas_tokrule rule);

// smae as ulas_tok but modifies line
int ulas_tokline(char *dst, const char **line, size_t n, ulas_tokrule rule);

#endif
