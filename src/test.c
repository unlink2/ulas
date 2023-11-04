#include "ulas.h"
#include <stdio.h>

void null_test_success(void) {
  puts("[null test]");
  puts("[null test ok]");
}

int main(int arc, char **argv) {
  ulas_init(ulas_cfg_from_env());

  if (!ulascfg.verbose) {
    fclose(stderr);
  }

  null_test_success();

  return 0;
}
