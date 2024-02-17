#ifndef ARCHS_H_
#define ARCHS_H_

enum ulas_asmregs_sm83 {
  // r8
  ULAS_REG_B = 1,
  ULAS_REG_C = 2,
  ULAS_REG_D = 3,
  ULAS_REG_E = 4,
  ULAS_REG_H = 5,
  ULAS_REG_L = 6,
  ULAS_REG_A = 7,

  // r16
  ULAS_REG_BC = 8,
  ULAS_REG_DE = 9,
  ULAS_REG_HL = 10,
  ULAS_REG_AF = 16,

  // flags
  ULAS_REG_NOT_ZERO = 11,
  ULAS_REG_ZERO = 12,
  ULAS_REG_NOT_CARRY = 13,
  ULAS_REG_CARRY = 14,

  // misc
  ULAS_REG_SP = 15,
  ULAS_VEC00 = 17,
  ULAS_VEC08 = 18,
  ULAS_VEC10 = 19,
  ULAS_VEC18 = 20,
  ULAS_VEC20 = 21,
  ULAS_VEC28 = 22,
  ULAS_VEC30 = 23,
  ULAS_VEC38 = 24
};

#endif
