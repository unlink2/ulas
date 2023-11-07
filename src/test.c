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
      puts(dst.buf);                                                           \
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

  assert_tok(
      "  test  tokens   with   line / * + - , ;",
      {"test", "tokens", "with", "line", "/", "*", "+", "-", ",", ";", NULL});

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
