#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ULAS_PATHMAX 4096
#define ULAS_LINEMAX 4096
#define ULAS_OUTBUFMAX 64
#define ULAS_MACROPARAMMAX 9

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
#define ULASDBG(...)                                                           \
  if (ulascfg.verbose) {                                                       \
    fprintf(ulaserr, __VA_ARGS__);                                             \
  }
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

/**
 * Output target files 
 */

// input file for source reader 
extern FILE *ulasin;
// source code output target 
extern FILE *ulasout;
// error output target
extern FILE *ulaserr;
// assembly listing output 
extern FILE *ulaslstout;
// symbol list output
extern FILE *ulassymout;

struct ulas_expr;
struct ulas_tok;

struct ulas_config {
  // argv represents file names
  char **argv;
  int argc;

  char *output_path;
  char *lst_path;
  char *sym_path;

  int verbose;
  int preproc_only;
};

/**
 * str
 */
struct ulas_str {
  char *buf;
  unsigned long maxlen;
};

/**
 * Tokens
 */

// any token before 256 is just the literal char value
// primitive data types
enum ulas_type {
  ULAS_SYMBOL = 256,
  ULAS_INT,
  ULAS_STR,
  ULAS_EQ,
  ULAS_NEQ,
  ULAS_GTEQ,
  ULAS_LTEQ
};

// data type value
union ulas_val {
  int intv;
  char *strv;
};

struct ulas_tok {
  enum ulas_type type;
  union ulas_val val;
};

// the token buffer is a dynamically allocated token store
struct ulas_tokbuf {
  struct ulas_tok *buf;
  long len;
  long maxlen;
};

// the expression buffer hold expression buffers
struct ulas_exprbuf {
  struct ulas_expr *buf;
  unsigned long len;
  unsigned long maxlen;
};

/**
 * Assembly context
 */

struct ulas {
  char *filename;
  unsigned long line;

  // holds the current token
  struct ulas_str tok;

  // current token stream
  struct ulas_tokbuf toks;
  struct ulas_exprbuf exprs;

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

struct ulas_preproc {
  struct ulas_ppdef *defs;
  unsigned long defslen;

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
  struct ulas_tok tok;
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

enum ulas_exprs { ULAS_EXPUN, ULAS_EXPBIN, ULAS_EXPPRIM, ULAS_EXPGRP };

struct ulas_expun {
  long right;
  long op;
};

struct ulas_expbin {
  long left;
  long right;
  long op;
};

struct ulas_expprim {
  long tok;
};

struct ulas_expgrp {
  // points to the first expression
  // in this group
  long head;
};

union ulas_expval {
  struct ulas_expun un;
  struct ulas_expbin bin;
  struct ulas_expprim prim;
  struct ulas_expgrp grp;
};

struct ulas_expr {
  enum ulas_exprs type;
  union ulas_expval val;
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

char *ulas_strndup(const char *src, unsigned long n);

// resolve a symbol until an actual literal token (str, int) is found
// returns NULL if the symbol cannot be resolved
struct ulas_tok *ulas_symbolresolve(const char *name);

// tokenisze according to pre-defined rules
// returns the amount of bytes of line that were
// consumed or -1 on error
// returns 0 when no more tokens can be read
int ulas_tok(struct ulas_str *dst, const char **out_line, unsigned long n);

// converts a token string to a token struct
struct ulas_tok ulas_totok(char *buf, unsigned long n, int *rc);

int ulas_tokuntil(struct ulas_str *dst, char c, const char **out_line,
                  unsigned long n);

/**
 * str
 */

// create a string buffer
struct ulas_str ulas_str(unsigned long n);

// ensure the string buffer is at least n bytes long, if not realloc
struct ulas_str ulas_strensr(struct ulas_str *s, unsigned long maxlen);

// require at least n bytes + the current strlen
struct ulas_str ulas_strreq(struct ulas_str *s, unsigned long n);

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
                     const char *raw_line, unsigned long n);

// expand preproc into dst line
char *ulas_preprocexpand(struct ulas_preproc *pp, const char *raw_line,
                         unsigned long *n);

/**
 * Literals, tokens and expressions
 */

// convert literal to its int value
int ulas_valint(struct ulas_tok *lit, int *rc);
// convert literal to its char value
char *ulas_valstr(struct ulas_tok *lit, int *rc);

struct ulas_tokbuf ulas_tokbuf(void);

// pushes new token, returns newly added index
int ulas_tokbufpush(struct ulas_tokbuf *tb, struct ulas_tok tok);
struct ulas_tok *ulas_tokbufget(struct ulas_tokbuf *tb, int i);
void ulas_tokbufclear(struct ulas_tokbuf *tb);
void ulas_tokbuffree(struct ulas_tokbuf *tb);

struct ulas_exprbuf ulas_exprbuf(void);

// pushes new expression, returns newly added index
int ulas_exprbufpush(struct ulas_exprbuf *eb, struct ulas_expr expr);
struct ulas_expr *ulas_exprbufget(struct ulas_exprbuf *eb, int i);
void ulas_exprbufclear(struct ulas_exprbuf *eb);
void ulas_exprbuffree(struct ulas_exprbuf *eb);

/**
 * Assembly step
 */

// returns 0 if no more data can be read
//         > 0 if data was read
//         -1 on error
int ulas_asmnext(FILE *dst, FILE *src, char *buf, int n);
int ulas_asm(FILE *dst, FILE *src);

// parses and executes a 32 bit signed int math expressions
int ulas_intexpr(const char **line, unsigned long n, int *rc);

#endif
