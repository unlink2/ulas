#include "uldas.h"

int ulas_dasm(FILE *src, FILE *dst) {
  // pass 1: run and collect labels 
  // pass 2: run and output to file
  
  // TODO: the instruction list should be sorted so we can search it faster
  // but for now we just do it as a linear search
  
  unsigned long srctell = ftell(src);
  
  int c = '\0';
  while (fgetc(src) != EOF) {
  }

  return 0;
}
