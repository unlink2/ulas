#include "uldas.h"

// dasm the next instruction
// if there are no more bytes to be read, return 0
// on error return -1
// otherwise return 1
int ulas_dasm_next(FILE *src, FILE *dst) {
  unsigned long srctell = ftell(src);
  // read n bytes (as many as in instruction)
  char buf[ULAS_OUTBUFMAX];
  memset(buf, 0, ULAS_OUTBUFMAX);
  unsigned long read = 0;

  // find the correct instructions
  // needs to match every byte!
  // first read max outbuf
  read = fread(buf, 1, ULAS_OUTBUFMAX, src);
  if (read == 0) {
    return 0;
  }

  // now find the instruction that matches all
  // read bytes
  // -> then reset src's read buffer to the srctell + actual instruction's
  // length if nothing matches simply output a .db for the first byte and return
  for (int i = 0; ulas.arch.instrs[i].name; i++) {
    const struct ulas_instr *instr = &ulas.arch.instrs[i];

    // test all instruction's contents
    for (int j = 0; instr->data[j]; j++) {
      int dat = instr->data[j];
      if (dat == ULAS_DATZERO) {
        dat = 0;
      }
    
      // do we even have enough data?
      if (j >= read) {
        break;
      }

      switch (dat) {
      case ULAS_E8:
      case ULAS_A8:
        break;
      case ULAS_E16:
      case ULAS_A16:
        break;
      default:
        break;
      }
    }
  }

  return 1;
}

// TODO: implement label generation
int ulas_dasm(FILE *src, FILE *dst) {
  // pass 1: run and collect labels
  // pass 2: run and output to file

  int rc = 0;
  while ((rc = ulas_dasm_next(src, dst)) > 0) {
    if (rc == -1) {
      return -1;
    }
  }

  return rc;
}
