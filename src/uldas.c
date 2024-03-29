#include "uldas.h"


void ulas_dasm_print_addr(FILE *dst) {
  if (ulascfg.print_addrs) {
    fprintf(dst, "%08x ", ulas.address);
  }
}

void ulas_dasm_print_header(FILE *dst) {
  if (ulas.pass != ULAS_PASS_FINAL) {
    return;
  }
  ulas_dasm_print_addr(dst);
  fprintf(dst, ".org 0x%x\n", ulas.address);
}

// this function assumes the bounds checks
// for buf have already been done and will not check anymore!
void ulas_dasm_instr_fout(FILE *src, FILE *dst, const struct ulas_instr *instr,
                          const char *buf, unsigned long read) {
  if (ulas.pass != ULAS_PASS_FINAL) {
    return;
  }

  ulas_dasm_print_addr(dst);

  fprintf(dst, "  %s ", instr->name);
  unsigned int bi = ulas_arch_opcode_len(buf, read);
  for (int i = 0; instr->tokens[i]; i++) {
    int dat = instr->tokens[i];
    switch (dat) {
    case ULAS_E8:
    case ULAS_A8:
      fprintf(dst, "0x%x", buf[bi++] & 0xFF);
      break;
    case ULAS_A16:
    case ULAS_E16: {
      unsigned short val = 0;
      if (ulas.arch.endianess == ULAS_BE) {
        val = (unsigned short)(buf[bi + 1] & 0xFF) | (unsigned short)((buf[bi] & 0xFF) << 8);
      } else {
        val = (unsigned short)(buf[bi] & 0xFF) | (unsigned short)((buf[bi + 1] & 0xFF) << 8);
      }
      bi += 2;
      fprintf(dst, "0x%x", val & 0xFFFF);
      break;
    }
    default: {
      const char *reg = ulas_asmregstr(dat);
      if (reg) {
        fprintf(dst, "%s", reg);
      } else {
        fprintf(dst, "%c", dat);
        // just for formatting purposes
        if (dat == ',') {
          fprintf(dst, " ");
        }
      }
      break;
    }
    }
  }

  fprintf(dst, "\n");
}

// fallback if no instruction was found
void ulas_dasm_db_fout(FILE *src, FILE *dst, const char *buf,
                       unsigned long read) {
  ulas.address++;
  if (ulas.pass != ULAS_PASS_FINAL) {
    return;
  }

  ulas_dasm_print_addr(dst);
  fprintf(dst, ".db 0x%x\n", buf[0] & 0xFF);
}

// returns > 1 if instruction was found, and 0 if not
// the integer returned is the amount of bytes this instruction consumed
int ulas_dasm_instr_check(FILE *src, FILE *dst, const struct ulas_instr *instr,
                          const char *buf, unsigned long read) {
  int i = 0;
  int bi = 0; // current buffer index
  // test all instruction's contents
  for (i = 0; instr->data[i]; i++) {
    unsigned int dat = instr->data[i];
    if (dat == ULAS_DATZERO) {
      dat = 0;
    }

    // do we even have enough data?
    // this is a general check for 1 byte
    if (bi + 1 >= read) {
      goto fail;
    }

    switch (dat) {
    case ULAS_E8:
    case ULAS_A8:
      // just ignore it
      bi++;
      break;
    case ULAS_E16:
    case ULAS_A16:
      // need 2 bytes here
      bi += 2;
      if (bi > read) {
        goto fail;
      }
      break;
    default:
      if ((buf[bi] & 0xFF) != (dat & 0xFF)) {
        goto fail;
      }
      bi++;
      break;
    }
  }

  ulas_dasm_instr_fout(src, dst, instr, buf, read);
  ulas.address += i;
  return bi;
fail:
  return 0;
}

#define ULAS_DASM_BUFMAX 4

// dasm the next instruction
// if there are no more bytes to be read, return 0
// on error return -1
// otherwise return 1
int ulas_dasm_next(FILE *src, FILE *dst) {
  long srctell = ftell(src);
  // read n bytes (as many as in instruction)
  char buf[ULAS_DASM_BUFMAX];
  memset(buf, 0, ULAS_DASM_BUFMAX);
  unsigned long read = 0;

  // find the correct instructions
  // needs to match every byte!
  // first read max outbuf
  read = fread(buf, 1, ULAS_DASM_BUFMAX, src);
  if (read == 0) {
    return 0;
  }

  // now find the instruction that matches all
  // read bytes
  // -> then reset src's read buffer to the srctell + actual instruction's
  // length if nothing matches simply output a .db for the first byte and return
  for (int i = 0; ulas.arch.instrs[i].name; i++) {
    const struct ulas_instr *instr = &ulas.arch.instrs[i];

    int consumed = ulas_dasm_instr_check(src, dst, instr, buf, read);
    if (consumed) {
      fseek(src, srctell + consumed, 0);
      return 1;
    }
  }

  ulas_dasm_db_fout(src, dst, buf, read);
  fseek(src, srctell + 1, 0);
  return 1;
}

// TODO: implement label generation
int ulas_dasm(FILE *src, FILE *dst) {
  // pass 1: run and collect labels
  // pass 2: run and output to file

  ulas_dasm_print_header(dst);

  int rc = 0;
  while ((rc = ulas_dasm_next(src, dst)) > 0) {
    if (rc == -1) {
      return -1;
    }
  }

  return rc;
}
