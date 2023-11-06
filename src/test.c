#include "ulas.h"
#include <stdio.h>
#include <assert.h>
#include "preproc.h"

#define TESTBEGIN(name) printf("[test %s]\n", (name));
#define TESTEND(name) printf("[%s ok]\n", (name));

#define assert_tok(expected_tok, expected_ret, line, rule)                     \
  {                                                                            \
    char buf[ULAS_TOKMAX];                                                     \
    memset(buf, 0, ULAS_TOKMAX);                                               \
    assert(ulas_tok(buf, (line), ULAS_TOKMAX, (rule)) == (expected_ret));      \
    assert(strcmp(buf, expected_tok) == 0);                                    \
  }

#define assert_tokline(expected_n, line, rule, ...)                            \
  {                                                                            \
    const char *expect[] = __VA_ARGS__;                                        \
    size_t n = ULAS_TOKMAX;                                                    \
    char buf[n];                                                               \
    memset(buf, 0, n);                                                         \
    int i = 0;                                                                 \
    const char *pline = line;                                                  \
    while (ulas_tokline(buf, &pline, n, rule)) {                               \
      assert(strcmp(buf, expect[i]) == 0);                                     \
      i++;                                                                     \
    }                                                                          \
    assert(i == expected_n);                                                   \
  }

void test_tok(void) {
  TESTBEGIN("tok");

  assert_tok("test", 4, "test tokens", ulas_tokrulespace);
  assert_tok("test", 6, "  test tokens", ulas_tokrulespace);
  assert_tok("tokens", 6, "tokens", ulas_tokrulespace);
  assert_tok("", 0, "", ulas_tokrulespace);
  assert_tok("", -1, NULL, ulas_tokrulespace);

  assert_tokline(4, "  test  tokens   with   line", ulas_tokrulespace,
                 {"test", "tokens", "with", "line"});

  TESTEND("tok");
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

  TESTEND("preproc");
}

int main(int arc, char **argv) {
  ulas_init(ulas_cfg_from_env());

  /*if (!ulascfg.verbose) {
    fclose(stderr);
  }*/

  test_tok();
  test_preproc();

  return 0;
}
