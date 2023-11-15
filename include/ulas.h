#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ULAS_PATHMAX 4096
#define ULAS_LINEMAX 4096

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

#define ULAS_PPSTR_DEF "#define"
#define ULAS_PPSTR_MACRO "#macro"
#define ULAS_PPSTR_IFDEF "#ifdef"
#define ULAS_PPSTR_ENDIF "#endif"
#define ULAS_PPSTR_IFNDEF "#ifndef"
#define ULAS_PPSTR_ENDMACRO "#endmacro"
#define ULAS_PPSTR_UNDEF "#undefine"

#define ULAS_ASMSTR_ORG ".org"
#define ULAS_ASMSTR_SET ".set"
#define ULAS_ASMSTR_BYTE ".db"
#define ULAS_ASMSTR_STR ".str"
#define ULAS_ASMSTR_FILL ".fill"
#define ULAS_ASMSTR_PAD ".pad"
#define ULAS_ASMSTR_INCBIN ".incbin"

// configurable tokens
#define ULAS_TOK_COMMENT ';'
// start of as directives such as .org
#define ULAS_TOK_ASMDIR_BEGIN '.'
// start of preprocessor directives such as #define or #include
#define ULAS_TOK_PREPROC_BEGIN '#'

#define ULASINFO() fprintf(ulaserr, "%s:%ld ", ulas.filename, ulas.line);
#define ULASERR(...) ULASINFO() fprintf(ulaserr, __VA_ARGS__);
#define ULASWARN(...) ULASINFO() fprintf(ulaserr, __VA_ARGS__);
#define ULASPANIC(...)                                                         \
  {                                                                            \
    ULASINFO();                                                                \
    fprintf(ulaserr, __VA_ARGS__);                                             \
    exit(-1);                                                                  \
  }

// format macros
#define ULAS_FMT(f, fmt)                                                       \
  if (isatty(fileno(f)) && ulascfg.color) {                                    \
    fprintf((f), "%s", (fmt));                                                 \
  }

extern FILE *ulasin;
extern FILE *ulasout;
extern FILE *ulaserr;

struct ulas_expr;
struct ulas_tok;

struct ulas_config {
  // argv represents file names
  char **argv;
  int argc;

  char *output_path;

  int verbose;
  int preproc_only;
};

/**
 * str
 */
struct ulas_str {
  char *buf;
  size_t maxlen;
};

/**
 * Tokens
 */

// any token before 256 is just the literal char value 
enum ulas_toks { ULAS_TOKLITERAL = 256, ULAS_TOKINT, ULAS_TOKCHAR, ULAS_TOKSTRING };

// primitive data types
enum ulas_type { ULAS_INT, ULAS_STR };

// data type value
union ulas_val {
  int int_value;
  char *str_value;
};

// literal value
struct ulas_lit {
  enum ulas_type type;
  union ulas_val val;
};

struct ulas_tok {
  enum ulas_toks type;
  struct ulas_lit lit;
};

// the token buffer is a dynamically allocated token store
struct ulas_tokbuf {
  struct ulas_tok *buf;
  long len;
  long maxlen;
};

/**
 * Assembly context
 */

struct ulas {
  char *filename;
  size_t line;

  // holds the current token
  struct ulas_str tok;

  // current token stream
  struct ulas_tokbuf toks;

  unsigned int address;

  // internal counter
  // used whenever a new unique number might be needed
  int icntr;
};

extern struct ulas ulas;

int ulas_icntr(void);

/**
 * Preproc
 */

enum ulas_ppdirs {
  ULAS_PPDIR_NONE = 1,
  // #define name value
  ULAS_PPDIR_DEF,
  // #undefine name
  ULAS_PPDIR_UNDEF,
  // #macro name
  // value
  // value
  ULAS_PPDIR_MACRO,
  // #endmacro
  ULAS_PPDIR_ENDMACRO,
  // ifdef name
  ULAS_PPDIR_IFDEF,
  // ifndef name
  ULAS_PPDIR_IFNDEF,
  // endif
  ULAS_PPDIR_ENDIF
};

enum ulas_ppdefs {
  ULAS_PPDEF,
  ULAS_PPMACRO,
};

struct ulas_ppdef {
  enum ulas_ppdefs type;
  char *name;
  char *value;
  int undef;
};

#define ULAS_MACROPARAMMAX 9

struct ulas_preproc {
  struct ulas_ppdef *defs;
  size_t defslen;

  struct ulas_str tok;
  struct ulas_str line;

  // macro parameter buffers
  struct ulas_str macroparam[ULAS_MACROPARAMMAX];
  // macro expansion buffer
  struct ulas_str macrobuf;
};

/**
 * Symbols
 */

struct ulas_sym {
  char *name;
  struct ulas_lit lit;
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
  struct ulas_expr *expr;
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

/**
 * asm
 */
enum ulas_asmdir {
  ULAS_ASMDIR_NONE = 0,
  // .org <address>
  ULAS_ASMDIR_ORG,
  // .set name = <expr>
  ULAS_ASMDIR_SET,
  // .byte <expr>, <expr>, <expr>, ...
  ULAS_ASMDIR_BYTE,
  // .str "String value"
  ULAS_ASMDIR_STR,
  // .fill <expr>, n
  ULAS_ASMDIR_FILL,
  // .pad <expr>, <addr>
  ULAS_ASMDIR_PAD,
  // .incbin <filename>
  ULAS_ASMDIR_INCBIN,
};

extern struct ulas_config ulascfg;

struct ulas_config ulas_cfg_from_env(void);
void ulas_init(struct ulas_config cfg);
void ulas_free(void);

int ulas_main(struct ulas_config cfg);

char *ulas_strndup(const char *src, size_t n);

// tokenisze according to pre-defined rules
// returns the amount of bytes of line that were
// consumed or -1 on error
// returns 0 when no more tokens can be read
int ulas_tok(struct ulas_str *dst, const char **out_line, size_t n);

// converts a token string to a token struct
struct ulas_tok ulas_totok(const char *buf, size_t n, int *rc);

int ulas_tokuntil(struct ulas_str *dst, char c, const char **out_line,
                  size_t n);

/**
 * str
 */

// create a string buffer
struct ulas_str ulas_str(size_t n);

// ensure the string buffer is at least n bytes long, if not realloc
struct ulas_str ulas_strensr(struct ulas_str *s, size_t maxlen);

// require at least n bytes + the current strlen
struct ulas_str ulas_strreq(struct ulas_str *s, size_t n);

void ulas_strfree(struct ulas_str *s);

/*
 * Preprocessor
 */

struct ulas_preproc ulas_preprocinit(void);
void ulas_preprocfree(struct ulas_preproc *pp);

/**
 * Tokenize and apply the preprocessor
 * returns 0: no error
 *        -1: error
 */
int ulas_preproc(FILE *dst, FILE *src);

// reads the next line
// returns 0 if no more data can be read
//         > 0 if data was read (enum ulas_ppdirs id)
//         -1 on error
// it also places the processed line into pp->line.buf
// note that this is overwritten by every call!
int ulas_preprocnext(struct ulas_preproc *pp, FILE *dst, FILE *src, char *buf,
                     int n);

// process a line of preproc
// returns: 0 when a regular line was read
//          enum ulas_ppdirs id for preprocessor directive
//          -1 on error
//  Warning: calling this recursively may clobber pp buffers and those should
//  not be used in the caller after recursvion finishes!
//  or initialize a new preproc object if the old state is important!
//  (preprocinit and preprocfree)
int ulas_preprocline(struct ulas_preproc *pp, FILE *dst, FILE *src,
                     const char *raw_line, size_t n);

// expand preproc into dst line
char *ulas_preprocexpand(struct ulas_preproc *pp, const char *raw_line,
                         size_t *n);

/**
 * Literals, tokens and expressions
 */

// convert literal to its int value
int ulas_litint(struct ulas_lit *lit, int *rc);
// convert literal to its char value
char *ulas_litchar(struct ulas_lit *lit, int *rc);

struct ulas_tokbuf ulas_tokbuf(void);
void ulas_tokbufpush(struct ulas_tokbuf *tb, struct ulas_tok tok);
void ulas_tokbufclear(struct ulas_tokbuf *tb);
void ulas_tokbuffree(struct ulas_tokbuf *tb);

/**
 * Assembly step
 */

// returns 0 if no more data can be read
//         > 0 if data was read
//         -1 on error
int ulas_asmnext(FILE *dst, FILE *src, char *buf, int n);
int ulas_asm(FILE *dst, FILE *src);

// parses and executes a 32 bit signed int math expressions
int ulas_intexpr(const char **line, size_t n, int *rc);

#endif
