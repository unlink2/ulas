#include "ulas.h"
#include <stdio.h>
#include <assert.h>

#define TESTBEGIN(name) printf("[test %s]\n", (name));
#define TESTEND(name) printf("[%s ok]\n", (name));

#define assert_tok(expected_tok, expected_ret, line, rule)                     \
  {                                                                            \
    char buf[256];                                                             \
    memset(buf, 0, 256);                                                       \
    assert(ulas_tok(buf, (line), 256, (rule)) == (expected_ret));              \
    assert(strcmp(buf, expected_tok) == 0);                                    \
  }

void test_tok(void) {
  TESTBEGIN("tok");

  assert_tok("test", 4, "test tokens", ulas_tokrulespace);
  assert_tok("tokens", 6, "tokens", ulas_tokrulespace);
  assert_tok("", 0, "", ulas_tokrulespace);
  assert_tok("", -1, NULL, ulas_tokrulespace);

  TESTEND("tok");
}

int main(int arc, char **argv) {
  ulas_init(ulas_cfg_from_env());

  if (!ulascfg.verbose) {
    fclose(stderr);
  }

  test_tok();

  return 0;
}
