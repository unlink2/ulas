#include "ulas.h"
#include <ctype.h>
#include <stdio.h>
#include <assert.h>

#define ULAS_TOKMAX 64

#define TESTBEGIN(name) printf("[test %s]\n", (name));
#define TESTEND(name) printf("[%s ok]\n", (name));

#define assert_tok(expected_tok, expected_ret, line, rule)                     \
  {                                                                            \
    struct ulas_str dst = ulas_str(ULAS_TOKMAX);                               \
    memset(dst.buf, 0, ULAS_TOKMAX);                                           \
    assert(ulas_tok(&dst, (line), ULAS_TOKMAX, (rule)) == (expected_ret));     \
    assert(strcmp(dst.buf, expected_tok) == 0);                                \
    ulas_strfree(&dst);                                                        \
  }

#define assert_tokline(expected_n, line, rule, ...)                            \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    size_t n = ULAS_TOKMAX;                                                    \
    struct ulas_str dst = ulas_str(n);                                         \
    memset(dst.buf, 0, n);                                                     \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tokline(&dst, &pline, n, rule)) {                              \
      assert(strcmp(dst.buf, expect[i]) == 0);                                 \
      i++;                                                                     \
    }                                                                          \
    assert(i == expected_n);                                                   \
    ulas_strfree(&dst);                                                        \
  }

void test_tok(void) {
  TESTBEGIN("tok");

  assert_tok("test", 4, "test tokens", isspace);
  assert_tok("test", 6, "  test tokens", isspace);
  assert_tok("tokens", 6, "tokens", isspace);
  assert_tok("", 0, "", isspace);
  assert_tok("", -1, NULL, isspace);

  assert_tokline(4, "  test  tokens   with   line", isspace,
                 {"test", "tokens", "with", "line"});

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
    assert(ulas_preproc(dst, "testdst", src, "testsrc") == (expect_ret));      \
    fclose(src);                                                               \
    fclose(dst);                                                               \
    assert(strcmp(dstbuf, (expect_dst)) == 0);                                 \
  }

void test_preproc(void) {
  TESTBEGIN("preproc");

  // should just echo back line as is
  assert_preproc("  test line", 0, "  test line");
  assert_preproc("", 0, "  #define test");

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
