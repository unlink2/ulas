#ifndef ULASH__
#define ULASH__

#include <stdbool.h>

struct ulas_config {
  char **argv;
  int argc;

  bool verbose;
};

int ulas_main(struct ulas_config *cfg);

#endif 
