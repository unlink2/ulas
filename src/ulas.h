#ifndef ULASH__
#define ULASH__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "archs.h"

// if this is used as a path use stdin or stdout instead
#define ULAS_STDFILEPATH "-"
#define ULAS_PATHSEP "/"

#define ULAS_CHARCODEMAPLEN 256

#define ULAS_SYMNAMEMAX 256
#define ULAS_PATHMAX 4096
#define ULAS_LINEMAX 4096
#define ULAS_OUTBUFMAX 64
#define ULAS_MACROPARAMMAX 15

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
#define ULAS_PPSTR_INCLUDE "#include"

#define ULAS_ASMSTR_ORG ".org"
#define ULAS_ASMSTR_SET ".set"
#define ULAS_ASMSTR_BYTE ".db"
#define ULAS_ASMSTR_STR ".str"
#define ULAS_ASMSTR_FILL ".fill"
#define ULAS_ASMSTR_PAD ".pad"
#define ULAS_ASMSTR_INCBIN ".incbin"
#define ULAS_ASMSTR_DEF ".def"
// TODO: chksm should only work on sm83
#define ULAS_ASMSTR_CHKSM ".chksm"
#define ULAS_ASMSTR_ADV ".adv"
#define ULAS_ASMSTR_SET_ENUM_DEF ".se"
#define ULAS_ASMSTR_DEFINE_ENUM ".de"
#define ULAS_ASMSTR_SETCHRCODE ".scc"
#define ULAS_ASMSTR_CHR ".chr"
#define ULAS_ASMSTR_REP ".rep"

// configurable tokens
#define ULAS_TOK_COMMENT ';'
// start of as directives such as .org
#define ULAS_TOK_ASMDIR_BEGIN '.'
// start of preprocessor directives such as #define or #include
#define ULAS_TOK_PREPROC_BEGIN '#'
#define ULAS_TOK_SCOPED_SYMBOL_BEGIN '@'
#define ULAS_TOK_CURRENT_ADDR '$'

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
#define ULASWARNLEVEL(level) (ulascfg.warn_level & (level))

// format macros
#define ULAS_FMT(f, fmt)                                                       \
  if (isatty(fileno(f)) && ulascfg.color) {                                    \
    fprintf((f), "%s", (fmt));                                                 \
  }

// this is a bit of a hack to get the int expression to evaluate anyway
// because expressions only work in the final pass
// Beware that this can cause unforseen writes to the file and should really
// only be uesd to evalulate an expression that needs to be evaled during
// all passes and nothing else!
#define ULAS_EVALEXPRS(...)                                                    \
  {                                                                            \
    int pass = ulas.pass;                                                      \
    ulas.pass = ULAS_PASS_FINAL;                                               \
    __VA_ARGS__;                                                               \
    ulas.pass = pass;                                                          \
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

enum ulas_warm { ULAS_WARN_OVERFLOW = 1, ULAS_WARN_ALL = 0x7FFFFFFF };

struct ulas_config {
  // argv represents file names
  char **argv;
  int argc;

  char *output_path;
  char *lst_path;
  char *sym_path;

  int verbose;
  int preproc_only;

  // all include search paths
  char **incpaths;
  unsigned int incpathslen;

  enum ulas_warm warn_level;
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
// FIXME: split up types and operators
// TODO:add float expressions
enum ulas_type {
  ULAS_SYMBOL = 256,
  ULAS_INT,
  ULAS_STR,
  ULAS_EQ,
  ULAS_NEQ,
  ULAS_GTEQ,
  ULAS_LTEQ,
  ULAS_RSHIFT,
  ULAS_LSHIFT,
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
 * The assembler can go over the code twice to resolve all future labels as well
 * as past labels This causes the entire process to start over for now meaning
 * that the 2 passes will behave as if they run their own assembly process with
 * slightly different settings. Pass settings ULAS_SYM_FINAL: Final pass, enable
 * code output and evaluate all expressions and directives ULAS_SYM_RESOLVE:
 * Disable code output and the .set directive. Resolve labels
 */
enum ulas_pass {
  ULAS_PASS_FINAL = 0,
  ULAS_PASS_RESOLVE = 1,
};

/**
 * Symbols
 */

struct ulas_sym {
  char *name;
  struct ulas_tok tok;
  // this label's scope index
  int scope;
  // last pass this was defined in...
  // a symbol may only be defined once per pass/scope
  enum ulas_pass lastdefin;
  int constant;
};

// holds all currently defned symbols
struct ulas_symbuf {
  struct ulas_sym *buf;
  unsigned long len;
  unsigned long maxlen;
};

/**
 * Assembly context
 */

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


struct ulas {
  struct ulas_preproc pp;
  char *filename;
  char *initial_filename;
  unsigned long line;

  // count how many passes we have completed so far
  int pass;

  // holds the current token
  struct ulas_str tok;

  // current token stream
  struct ulas_tokbuf toks;
  struct ulas_exprbuf exprs;
  struct ulas_symbuf syms;

  unsigned int address;
  int enumv;

  // current scope index
  // each global-label increments the scope
  int scope;

  // internal counter
  // used whenever a new unique number might be needed
  int icntr;

  char chksm;

  // character code map
  // defaults to just x=x mapping
  // but cna be set with a directive
  char charcodemap[ULAS_CHARCODEMAPLEN];

  struct ulas_arch arch;
};

extern struct ulas ulas;

int ulas_icntr(void);

// init the next pass
// by resetting some values
void ulas_nextpass(void);

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
  ULAS_PPDIR_ENDIF,
  // include "filename"
  ULAS_PPDIR_INCLUDE,
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
  // .set <type> name = <expr>
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
  // .def name = value
  ULAS_ASMDIR_DEF,
  // inserts checksum into rom
  ULAS_ASMDIR_CHKSM,
  // .adv <int>
  // advance .org by n bytes without writing to rom
  ULAS_ASMDIR_ADV,
  // .setenum <int>
  // sets the internal _ENUM counter
  ULAS_ASMDIR_SET_ENUM_DEF,
  // .de <size>
  // acts like .def but sets the value to the current _ENUM counter
  // and increments it by size
  ULAS_ASMDIR_DEFINE_ENUM,
  // .scc '<char>'=<value>
  // allows to set custom encoding for char codes
  // when using .str
  ULAS_ASMDIR_SETCHRCODE,

  // .chr 01230123
  // allows defining
  // chr (tile) data from 0-3
  // for each possible color
  // it requires up to 8 integer expressions between 0 and 3
  ULAS_ASMDIR_CHR,
  // .rep <n>, <step>, <line>
  // repeats a line n times
  ULAS_ASMDIR_REP,
};

// special asm tokens for instr enum
// TODO: add more expressions types such as e8, e16, e24, e32, e64
// as well as the corresponding addresses
enum ulas_asmspetok {
  ULAS_E8 = -1,
  ULAS_E16 = -2,
  // A8 is like E8, but it will not emit an overflow warning
  // because it is commont to pass a 16-bit label as a value
  ULAS_A8 = -3,
  ULAS_DATZERO = 0xFF00
};

#define ULAS_INSTRTOKMAX 16
#define ULAS_INSTRDATMAX 16

// entry for lut instruction parser
// in the form of: name token token token token
// tokens can either be literal chars, or any of the special tokens like an
// asmreg or e8, e16
// if toks value is 0 the list ends
// if data value is 0 the list ends
// if toks is E8 or E16 an expression is evaluated
// id data is E8 or E16 the previous expression is inserted
struct ulas_instr {
  char *name;
  short tokens[ULAS_INSTRTOKMAX];
  short data[ULAS_INSTRDATMAX];
};

extern struct ulas_config ulascfg;

struct ulas_config ulas_cfg_from_env(void);
void ulas_init(struct ulas_config cfg);
void ulas_free(void);
FILE *ulas_incpathfopen(const char *path, const char *mode);

int ulas_main(struct ulas_config cfg);

char *ulas_strndup(const char *src, unsigned long n);

// resolve a symbol until an actual literal token (str, int) is found
// returns NULL if the symbol cannot be resolved
// returns -1 if any flagged symbol was not found
// if flagged symbols remain unresolved (e.g. global or locals) rc is set to the
// respective flag value
struct ulas_sym *ulas_symbolresolve(const char *name, int scope, int *rc);

// define a new symbol
// scope 0 indicates global scope. a scope of -1 instructs
// the function to auto-detect the scope
// if a label starts with @ the current scope is used, otherwise 0 is used
// if the symbol already exists -1 is returned
int ulas_symbolset(const char *cname, int scope, struct ulas_tok tok,
                   int constant);

int ulas_symbolout(FILE *dst, struct ulas_sym *s);

// tokenisze according to pre-defined rules
// returns the amount of bytes of line that were
// consumed or -1 on error
// returns 0 when no more tokens can be read
// writes the token to dst string buffer
int ulas_tok(struct ulas_str *dst, const char **out_line, unsigned long n);

// converts a token string to a token struct
// this is only useful if we do not require the token literal
// but rather can be used to store a slimmer list of token types
// and literal values
struct ulas_tok ulas_totok(char *buf, unsigned long n, int *rc);

// tokenize until a terminator char is reached
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
void ulas_preprocclear(struct ulas_preproc *pp);
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
// retunrs -1 on error, 0 on success and 1 if there is an unresolved symbol
int ulas_valint(struct ulas_tok *lit, int *rc);
// convert literal to its char value
char *ulas_valstr(struct ulas_tok *lit, int *rc);

struct ulas_tokbuf ulas_tokbuf(void);

// TODO: maybe we could macro all those buf functions into a single more
// "geneirc" thing...

// pushes new token, returns newly added index
int ulas_tokbufpush(struct ulas_tokbuf *tb, struct ulas_tok tok);
struct ulas_tok *ulas_tokbufget(struct ulas_tokbuf *tb, int i);
void ulas_tokbufclear(struct ulas_tokbuf *tb);
void ulas_tokbuffree(struct ulas_tokbuf *tb);
void ulas_tokfree(struct ulas_tok *t);

struct ulas_exprbuf ulas_exprbuf(void);

// pushes new expression, returns newly added index
int ulas_exprbufpush(struct ulas_exprbuf *eb, struct ulas_expr expr);
struct ulas_expr *ulas_exprbufget(struct ulas_exprbuf *eb, int i);
void ulas_exprbufclear(struct ulas_exprbuf *eb);
void ulas_exprbuffree(struct ulas_exprbuf *eb);

struct ulas_symbuf ulas_symbuf(void);
int ulas_symbufpush(struct ulas_symbuf *sb, struct ulas_sym sym);
struct ulas_sym *ulas_symbufget(struct ulas_symbuf *sb, int i);
void ulas_symbufclear(struct ulas_symbuf *sb);
void ulas_symbuffree(struct ulas_symbuf *sb);

/**
 * Assembly step
 */

// assembles an instruction, writes bytes into dst
// returns bytes written or -1 on error
int ulas_asminstr(char *dst, unsigned long max, const char **line,
                  unsigned long n);

// returns 0 if no more data can be read
//         > 0 if data was read
//         -1 on error
int ulas_asmnext(FILE *dst, FILE *src, char *buf, int n);
int ulas_asm(FILE *dst, FILE *src);
int ulas_asmline(FILE *dst, FILE *src, const char *line, unsigned long n);

// parses and executes a 32 bit signed int math expressions
int ulas_intexpr(const char **line, unsigned long n, int *rc);
char *ulas_strexpr(const char **line, unsigned long n, int *rc);

#endif
