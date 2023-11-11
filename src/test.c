#include "ulas.h"
#include <ctype.h>
#include <stdio.h>
#include <assert.h>

#define ULAS_TOKMAX 64

#define TESTBEGIN(name) printf("[test %s]\n", (name));
#define TESTEND(name) printf("[%s ok]\n", (name));

#define assert_tok(line, ...)                                                  \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    size_t n = strlen(line);                                                   \
    struct ulas_str dst = ulas_str(n);                                         \
    memset(dst.buf, 0, n);                                                     \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tok(&dst, &pline, n)) {                                        \
      assert(expect[i]);                                                       \
      assert(strcmp(dst.buf, expect[i]) == 0);                                 \
      i++;                                                                     \
    }                                                                          \
    size_t expect_n = 0;                                                       \
    for (expect_n = 0; expect[expect_n]; expect_n++) {                         \
    }                                                                          \
    assert(i == expect_n);                                                     \
    ulas_strfree(&dst);                                                        \
  }

#define assert_tokuntil(line, c, ...)                                          \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    size_t n = strlen(line);                                                   \
    struct ulas_str dst = ulas_str(n);                                         \
    memset(dst.buf, 0, n);                                                     \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tokuntil(&dst, c, &pline, n)) {                                \
      assert(expect[i]);                                                       \
      assert(strcmp(dst.buf, expect[i]) == 0);                                 \
      i++;                                                                     \
    }                                                                          \
    size_t expect_n = 0;                                                       \
    for (expect_n = 0; expect[expect_n]; expect_n++) {                         \
    }                                                                          \
    assert(i == expect_n);                                                     \
    ulas_strfree(&dst);                                                        \
  }

void test_tok(void) {
  TESTBEGIN("tok");

  assert_tok("  test  tokens   with,   line / * + - , ; $1",
             {"test", "tokens", "with", ",", "line", "/", "*", "+", "-", ",",
              ";", "$1", NULL});

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
  TESTBEGIN("preproc");

  // no directive
  assert_preproc("  test line", 0, "  test line");

  // define
  assert_preproc("123", 0, "  #define test 123\ntest");
  assert_preproc("", -1, "  #define 1test 123\n");
  assert_preproc("", -1, "  #define\n");
  assert_preproc("this is a 123 for defs", 0,
                 "  #define test 123\nthis is a test for defs");

  // undefined
  assert_preproc("123\ntest", 0,
                 "#define test 123\ntest\n#undefine test\ntest");

  // macro
  assert_preproc(
      "  line p1 1 label01,2\n  line p2 2\n  line p3 3 p1, p2, p3\n", 0,
      "#macro test\n  line $1 1 label$$$$,$$\n  line $2 2\n  line $3 3 "
      "$0\n#endmacro\ntest p1, p2, p3");
  assert_preproc("test macro with no args\n", 0,
                 "#macro test\ntest macro with no args\n#endmacro\ntest");
  assert_preproc("", -1, "#macro test\n not terminated\n");
  assert_preproc(
      "nested macro t1\nafter\ncontent n1\n\n", 0,
      "#macro test\nnested macro $1\n#macro "
      "nested\ncontent $1\n#endmacro\nafter\nnested n1\n#endmacro\ntest t1");

  TESTEND("preproc");
}

int main(int arc, char **argv) {
  ulas_init(ulas_cfg_from_env());

  /*if (!ulascfg.verbose) {
    fclose(stderr);
  }*/

  test_tok();
  test_strbuf();
  test_preproc();

  return 0;
}
