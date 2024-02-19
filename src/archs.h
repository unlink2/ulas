#ifndef ARCHS_H_
#define ARCHS_H_

enum ulas_asmregs_sm83 {
  // r8
  ULAS_REGSM83_B = 1,
  ULAS_REGSM83_C = 2,
  ULAS_REGSM83_D = 3,
  ULAS_REGSM83_E = 4,
  ULAS_REGSM83_H = 5,
  ULAS_REGSM83_L = 6,
  ULAS_REGSM83_A = 7,

  // r16
  ULAS_REGSM83_BC = 8,
  ULAS_REGSM83_DE = 9,
  ULAS_REGSM83_HL = 10,
  ULAS_REGSM83_AF = 16,

  // flags
  ULAS_REGSM83_NOT_ZERO = 11,
  ULAS_REGSM83_ZERO = 12,
  ULAS_REGSM83_NOT_CARRY = 13,
  ULAS_REGSM83_CARRY = 14,

  // misc
  ULAS_REGSM83_SP = 15,
  ULAS_VECSM83_00 = 17,
  ULAS_VECSM83_08 = 18,
  ULAS_VECSM83_10 = 19,
  ULAS_VECSM83_18 = 20,
  ULAS_VECSM83_20 = 21,
  ULAS_VECSM83_28 = 22,
  ULAS_VECSM83_30 = 23,
  ULAS_VECSM83_38 = 24,

  ULAS_SM83_REGS_LEN
};

enum ulas_archs { ULAS_ARCH_SM83 };

struct ulas_arch {
  const char **regs_names;
  unsigned long regs_len;

  const struct ulas_instr *instrs;
};

void ulas_arch_set(enum ulas_archs arch);

#endif
