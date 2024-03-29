#include "ulas.h"
#include <stdio.h>
#include <assert.h>

#define ULAS_TOKMAX 64
#define TESTBEGIN(name) printf("[test %s]\n", (name));
#define TESTEND(name) printf("[%s ok]\n", (name));

#define assert_tok(line, ...)                                                  \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    unsigned long n = strlen(line);                                            \
    struct ulas_str dst = ulas_str(n);                                         \
    memset(dst.buf, 0, n);                                                     \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tok(&dst, &pline, n)) {                                        \
      assert(expect[i]);                                                       \
      assert(strcmp(dst.buf, expect[i]) == 0);                                 \
      i++;                                                                     \
    }                                                                          \
    unsigned long expect_n = 0;                                                \
    for (expect_n = 0; expect[expect_n]; expect_n++) {                         \
    }                                                                          \
    assert(i == expect_n);                                                     \
    ulas_strfree(&dst);                                                        \
  }

#define assert_tokuntil(line, c, ...)                                          \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    unsigned long n = strlen(line);                                            \
    struct ulas_str dst = ulas_str(n);                                         \
    memset(dst.buf, 0, n);                                                     \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tokuntil(&dst, c, &pline, n)) {                                \
      assert(expect[i]);                                                       \
      assert(strcmp(dst.buf, expect[i]) == 0);                                 \
      i++;                                                                     \
    }                                                                          \
    unsigned long expect_n = 0;                                                \
    for (expect_n = 0; expect[expect_n]; expect_n++) {                         \
    }                                                                          \
    assert(i == expect_n);                                                     \
    ulas_strfree(&dst);                                                        \
  }

void test_tok(void) {
  TESTBEGIN("tok");

  assert_tok("  test  tokens   with,   line / * + - , ; $1 $ = == != > < >= <=",
             {"test", "tokens", "with", ",", "line", "/",  "*",
              "+",    "-",      ",",    ";", "$1",   "$",  "=",
              "==",   "!=",     ">",    "<", ">=",   "<=", NULL});

  assert_tokuntil(" this is a, test for tok , until", ',',
                  {"this is a", "test for tok ", "until", NULL});

  TESTEND("tok");
}

void test_strbuf(void) {
  TESTBEGIN("strbuf");

  struct ulas_str s = ulas_str(5);
  assert(s.maxlen == 5);
  assert(s.buf);

  s = ulas_strensr(&s, 10);
  assert(s.maxlen == 10);
  assert(s.buf);

  ulas_strfree(&s);

  TESTEND("strbuf");
}

#define assert_preproc(expect_dst, expect_ret, input)                          \
  {                                                                            \
    ulas_preprocclear(&ulas.pp);                                               \
    ulas.pass = ULAS_PASS_RESOLVE;                                             \
    char dstbuf[ULAS_LINEMAX];                                                 \
    memset(dstbuf, 0, ULAS_LINEMAX);                                           \
    FILE *src = fmemopen((input), strlen((input)), "re");                      \
    FILE *dst = fmemopen(dstbuf, ULAS_LINEMAX, "we");                          \
    assert(ulas_preproc(dst, src) == (expect_ret));                            \
    fclose(src);                                                               \
    fclose(dst);                                                               \
    assert(strcmp(dstbuf, (expect_dst)) == 0);                                 \
  }

void test_preproc(void) {
  ulascfg.preproc_only = 1;
  TESTBEGIN("preproc");

  // no directive
  assert_preproc("  test line", 0, "  test line");

  // define
  assert_preproc("123", 0, "  #define test 123\ntest");
  assert_preproc("this is a ", 0, "  #define test\nthis is a test");
  assert_preproc("", -1, "  #define 1test 123\n");
  assert_preproc("", -1, "  #define\n");
  assert_preproc("this is a 123 for defs", 0,
                 "  #define test 123\nthis is a test for defs");
  // define used inside define
  assert_preproc(
      "this is a 123 for defs", 0,
      "  #define ftest 123\n#define test ftest\nthis is a test for defs");

  // undefined
  assert_preproc("123\ntest", 0,
                 "#define test 123\ntest\n#undefine test\ntest");

  // macro
  assert_preproc(
      "  line p1 1 label01,2 3\n  line p2 2\n  line p3 3 p1, p2, p3\n", 0,
      "#macro test\n  line $1 1 label$$$$,$$ $$\n  line $2 2\n  line $3 3 "
      "$0\n#endmacro\ntest p1, p2, p3");
  assert_preproc("test macro with no args\n", 0,
                 "#macro test\ntest macro with no args\n#endmacro\ntest");
  assert_preproc("", -1, "#macro test\n not terminated\n");
  assert_preproc(
      "nested macro t1\nafter\ncontent n1\n", 0,
      "#macro test\nnested macro $1\n#macro "
      "nested\ncontent $1\n#endmacro\nafter\nnested n1\n#endmacro\ntest t1");

  // this macro caused a heap buffer overflow in production code
  assert_preproc("ld a, verylongmacroinput & 0xFF\nld [hl+], a\nld a, "
                 "(verylongmacroinput >> 8) & 0xFF\nld [hl+], a\n",
                 0,
                 "#macro testlonginput\nld a, $1 & 0xFF\nld [hl+], a\nld a, "
                 "($1 >> 8) & 0xFF\nld [hl+], a\n#endmacro\ntestlonginput "
                 "verylongmacroinput");

  // ifdef
  assert_preproc(
      "before\nifdeftest defined!\nafter", 0,
      "before\n#define test\n#ifdef test\nifdeftest defined!\n#endif\nafter");
  assert_preproc("before\nafter", 0,
                 "before\n#ifdef test\nifdeftest defined!\n#endif\nafter");
  assert_preproc("ifdeftest defined!\n", -1,
                 "#define test\n#ifdef test\nifdeftest defined!\n");

  // ifndef
  assert_preproc("before\nifndeftest defined!\nafter", 0,
                 "before\n#ifndef test\nifndeftest defined!\n#endif\nafter");
  assert_preproc(
      "before\nafter", 0,
      "before\n#define test\n#ifndef test\nifndeftest defined!\n#endif\nafter");
  assert_preproc("ifndeftest defined!\n", -1,
                 "#ifndef test\nifndeftest defined!\n");

  TESTEND("preproc");
  ulascfg.preproc_only = 0;
}

#define ASSERT_INT_TOTOK(expected_val, expected_rc, token)                     \
  {                                                                            \
    int rc = 0;                                                                \
    struct ulas_tok tok = ulas_totok((token), strlen(token), &rc);             \
    assert((expected_rc) == rc);                                               \
    assert(tok.type == ULAS_INT);                                              \
    assert(tok.val.intv == (expected_val));                                    \
  }

#define ASSERT_STR_TOTOK(expected_val, expected_rc, token)                     \
  {                                                                            \
    int rc = 0;                                                                \
    struct ulas_tok tok = ulas_totok((token), strlen(token), &rc);             \
    assert((expected_rc) == rc);                                               \
    assert(tok.type == ULAS_STR);                                              \
    assert(strcmp((expected_val), tok.val.strv) == 0);                         \
    free(tok.val.strv);                                                        \
  }

#define ASSERT_SYMBOL_TOTOK(expected_val, expected_rc, token)                  \
  {                                                                            \
    int rc = 0;                                                                \
    struct ulas_tok tok = ulas_totok((token), strlen(token), &rc);             \
    assert((expected_rc) == rc);                                               \
    assert(tok.type == ULAS_SYMBOL);                                           \
    assert(strcmp((expected_val), tok.val.strv) == 0);                         \
    free(tok.val.strv);                                                        \
  }

#define ASSERT_UNEXPECTED_TOTOK(expected_rc, token)                            \
  {                                                                            \
    int rc = 0;                                                                \
    ulas_totok((token), strlen(token), &rc);                                   \
    assert((expected_rc) == rc);                                               \
  }

#define ASSERT_TOTOK(expected_val, expected_rc, token)                         \
  {                                                                            \
    int rc = 0;                                                                \
    struct ulas_tok tok = ulas_totok((token), strlen(token), &rc);             \
    assert((expected_rc) == rc);                                               \
    assert(tok.type == (expected_val));                                        \
    free(tok.val.strv);                                                        \
  }

void test_totok(void) {
  TESTBEGIN("totok");

  // regular ints
  ASSERT_INT_TOTOK(10, 0, "10");
  ASSERT_INT_TOTOK(0x1A, 0, "0x1A");
  ASSERT_INT_TOTOK(5, 0, "0b101");

  // chars
  ASSERT_INT_TOTOK('a', 0, "'a'");
  ASSERT_INT_TOTOK('\n', 0, "'\\n'");
  ASSERT_INT_TOTOK('\\', 0, "'\\\\'");
  // char - not terminated
  ASSERT_INT_TOTOK('a', -1, "'a");
  // bad escape
  ASSERT_INT_TOTOK(0, -1, "'\\z'");
  // unterminated escape
  ASSERT_INT_TOTOK('\n', -1, "'\\n");

  // string token
  ASSERT_STR_TOTOK("test", 0, "\"test\"");
  ASSERT_STR_TOTOK("test\n", 0, "\"test\\n\"");
  // string with escape
  ASSERT_STR_TOTOK("test\n\"123\"", 0, "\"test\\n\\\"123\\\"\"");
  // unterminated string
  ASSERT_STR_TOTOK("test\n\"123\"", -1, "\"test\\n\\\"123\\\"");

  // symbols
  ASSERT_SYMBOL_TOTOK("_symbol123", 0, "_symbol123");
  ASSERT_SYMBOL_TOTOK("symbol123", 0, "symbol123");

  ASSERT_UNEXPECTED_TOTOK(-1, "1symbol123");

  // generic tokens with no value
  ASSERT_TOTOK(ULAS_EQ, 0, "==");
  ASSERT_TOTOK(ULAS_NEQ, 0, "!=");
  ASSERT_TOTOK('=', 0, "=");
  ASSERT_TOTOK('+', 0, "+");
  ASSERT_TOTOK('!', 0, "!");

  TESTEND("totok");
}

#define ASSERT_INTEXPR(expected_val, expected_rc, expr)                        \
  {                                                                            \
    int rc = 0;                                                                \
    const char *oexpr = expr;                                                  \
    ulas.pass = ULAS_PASS_FINAL;                                               \
    int val = ulas_intexpr(&oexpr, strlen((expr)), &rc);                       \
    assert(rc == (expected_rc));                                               \
    assert(val == (expected_val));                                             \
  }

void test_intexpr(void) {
  TESTBEGIN("intexpr");

  ASSERT_INTEXPR(-1, -1, "");

  ASSERT_INTEXPR(1, 0, "1");

  ASSERT_INTEXPR(1, 0, "2 == 2");
  ASSERT_INTEXPR(0, 0, "2 == 2 == 3");
  ASSERT_INTEXPR(1, 0, "2 == 2 == 1");

  ASSERT_INTEXPR(1, 0, "2 < 3");
  ASSERT_INTEXPR(0, 0, "2 > 3");
  ASSERT_INTEXPR(1, 0, "3 <= 3");
  ASSERT_INTEXPR(1, 0, "3 >= 3");

  ASSERT_INTEXPR(5, 0, "2 + 3");
  ASSERT_INTEXPR(-1, 0, "2 + 3 - 6");

  ASSERT_INTEXPR(6, 0, "2 * 3");
  ASSERT_INTEXPR(2, 0, "8 / 4");
  ASSERT_INTEXPR(0, -1, "8 / 0");
  ASSERT_INTEXPR(0, -1, "8 % 0");
  ASSERT_INTEXPR(0, 0, "8 % 4");

  ASSERT_INTEXPR(0, 0, "!1");
  ASSERT_INTEXPR(-1, 0, "-1");
  ASSERT_INTEXPR(1, 0, "--1");
  ASSERT_INTEXPR(2, 0, "1 - -1");
  ASSERT_INTEXPR(17, 0, "2 + 3 * 5");

  ASSERT_INTEXPR(-1, -1, "((2 + 3) * 5");
  ASSERT_INTEXPR(25, 0, "(2 + 3) * 5");
  ASSERT_INTEXPR(4, 0, "1 + 3 ; comment");

  ASSERT_INTEXPR(3 & 2, 0, "3 & 2");
  ASSERT_INTEXPR(3 | 2, 0, "3 | 2");
  ASSERT_INTEXPR(3 ^ 2, 0, "3 ^ 2");
  ASSERT_INTEXPR(~2, 0, "~2");
  ASSERT_INTEXPR(3 << 2, 0, "3 << 2");
  ASSERT_INTEXPR(3 >> 2, 0, "3 >> 2");

  TESTEND("intexpr");
}

#define ASSERT_STREXPR(expected_val, expected_rc, expr)                        \
  {                                                                            \
    int rc = 0;                                                                \
    const char *oexpr = expr;                                                  \
    ulas.pass = ULAS_PASS_FINAL;                                               \
    const char *val = ulas_strexpr(&oexpr, strlen((expr)), &rc);               \
    assert(rc == (expected_rc));                                               \
    assert(val);                                                               \
    assert(strcmp((expected_val), val) == 0);                                  \
  }

void test_strexpr(void) {
  TESTBEGIN("strexpr");

  ASSERT_STREXPR("test", 0, "\"test\"");
  ASSERT_STREXPR("test\"", 0, "\"test\\\"\"");
  ASSERT_STREXPR("test\" 123", 0, "\"test\\\" 123\"");

  TESTEND("strexpr");
}

#define ASSERT_ASMINSTR(expect_len, line, ...)                                 \
  {                                                                            \
    const char *l = line;                                                      \
    int n = strlen(l);                                                         \
    char dst[64];                                                              \
    unsigned char expect_dst[] = {__VA_ARGS__};                                \
    int res = ulas_asminstr(dst, 64, &l, n);                                   \
    assert(res == expect_len);                                                 \
    for (int i = 0; i < res; i++) {                                            \
      assert(expect_dst[i] == (unsigned char)dst[i]);                          \
    }                                                                          \
  }

void test_asminstr(void) {
  TESTBEGIN("asminstr");

  ASSERT_ASMINSTR(1, "nop", 0x00);
  ASSERT_ASMINSTR(1, "halt", 0x76);
  ASSERT_ASMINSTR(1, "ld b, c", 0x41);
  ASSERT_ASMINSTR(2, "ldh a, [1 + 2]", 0xF0, 0x03);

  TESTEND("asminstr");
}

#define ASSERT_SYMSCOPE(expect_ret, name, scope, constant)                     \
  {                                                                            \
    struct ulas_tok tok = {ULAS_INT, {0}};                                     \
    assert(ulas_symbolset((name), (scope), tok, constant) == (expect_ret));    \
  }

void test_symscope(void) {
  TESTBEGIN("symscope");

  // auto-scope
  ASSERT_SYMSCOPE(0, "t1:", -1, 1);
  ASSERT_SYMSCOPE(-1, "t1:", -1, 1);
  ASSERT_SYMSCOPE(0, "@t1:", -1, 1);
  ASSERT_SYMSCOPE(-1, "@t1:", -1, 1);

  // manual scoping
  ASSERT_SYMSCOPE(0, "t2:", 5, 1);
  ASSERT_SYMSCOPE(-1, "t2:", 5, 1);
  ASSERT_SYMSCOPE(0, "t2:", 6, 1);
  ASSERT_SYMSCOPE(-1, "t2:", 6, 1);

  // set
  ASSERT_SYMSCOPE(0, "t3:", -1, 0);
  ASSERT_SYMSCOPE(0, "t3:", -1, 0);

  TESTEND("symscope");
}

#define ULAS_FULLEN 0xFFFF

#define ASSERT_FULL(expect_rc, in_path, expect_path)                           \
  {                                                                            \
    printf("[source: %s; expect: %s]\n", in_path, expect_path);                \
    ulaslstout = stdout;                                                       \
    ulassymout = stdout;                                                       \
    cfg.verbose = 1;                                                           \
    char dstbuf[ULAS_FULLEN];                                                  \
    char expect[ULAS_FULLEN];                                                  \
    FILE *expectf = fopen(expect_path, "re");                                  \
    int expect_len = fread(expect, 1, ULAS_FULLEN, expectf);                   \
    fclose(expectf);                                                           \
    memset(dstbuf, 0, ULAS_FULLEN);                                            \
    ulasout = fmemopen(dstbuf, ULAS_FULLEN, "we");                             \
    ulasin = fopen(in_path, "re");                                             \
    assert(ulas_main(cfg) == (expect_rc));                                     \
    fclose(ulasout);                                                           \
    for (int i = 0; i < expect_len; i++) {                                     \
      assert(expect[i] == dstbuf[i]);                                          \
      if (cfg.disas) {                                                         \
        putchar(dstbuf[i]); /* for easier debugging in disas mode */           \
      }                                                                        \
    }                                                                          \
    for (int i = expect_len; i < ULAS_FULLEN; i++) {                           \
      assert(dstbuf[i] == 0);                                                  \
    }                                                                          \
    ulasin = stdin;                                                            \
    ulasout = stdout;                                                          \
  }

#define ASSERT_FULL_ASM(expect_rc, in_path, expect_path)                       \
  {                                                                            \
    struct ulas_config cfg = ulas_cfg_from_env();                              \
    ASSERT_FULL(expect_rc, in_path, expect_path)                               \
  }

// tests the entire stack
void test_full_asm(void) {
  TESTBEGIN("testfullasm");

  ASSERT_FULL_ASM(0, "tests/t0.s", "tests/t0.bin");

  TESTEND("testfullasm");
}

#define ASSERT_FULL_DASM(expect_rc, in_path, expect_path)                      \
  {                                                                            \
    struct ulas_config cfg = ulas_cfg_from_env();                              \
    cfg.disas = 1;                                                             \
    ASSERT_FULL(expect_rc, in_path, expect_path)                               \
  }

void test_full_dasm(void) {
  TESTBEGIN("testfulldasm");

  ASSERT_FULL_DASM(0, "tests/t0.bin", "tests/t0_dasm.s");

  TESTEND("testfulldasm");
}

int main(int arc, char **argv) {
  TESTBEGIN("ulas test");
  ulas_init(ulas_cfg_from_env());

  /*if (!ulascfg.verbose) {
    fclose(stderr);
  }*/

  test_tok();
  test_strbuf();
  test_preproc();
  test_totok();
  test_intexpr();
  test_strexpr();
  test_asminstr();
  test_symscope();

  ulas_free();

  // this will re-init everything on its own,
  // so call after free
  test_full_dasm();
  test_full_asm();

  TESTEND("ulas test");
  return 0;
}
