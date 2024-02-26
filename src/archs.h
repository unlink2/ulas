#ifndef ARCHS_H_
#define ARCHS_H_

#define ULAS_INSTRDATMAX_VAL 0xFFFF

enum ulas_endianess {
  ULAS_BE,
  ULAS_LE
};

/**
 * Insturction table:
 * It is simply a table of all possible instructions for
 * the architecture.
 * 0 means the list has ended
 */

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

// special asm tokens for instr enum
// TODO: add more expressions types such as e8, e16, e24, e32, e64
// as well as the corresponding addresses
// Use ULAS_DATZERO if an actual byte is expected to appear in the assembly
// output (since 0 denotes the end of the list)
enum ulas_asmspetok {
  ULAS_E8 = -1,
  ULAS_E16 = -2,
  // A8 is like E8, but it will not emit an overflow warning
  // because it is commont to pass a 16-bit label as a value
  ULAS_A8 = -3,
  ULAS_A16 = -4,
  ULAS_DATZERO = 0xFF00
};

enum ulas_archs { ULAS_ARCH_SM83 };

struct ulas_arch {
  enum ulas_archs type;
  const char **regs_names;
  unsigned long regs_len;

  const struct ulas_instr *instrs;
  enum ulas_endianess endianess;
};

void ulas_arch_set(enum ulas_archs arch);

// returns how many bytes of an instruction are occupied 
// by the opcode based on its data 
unsigned int ulas_arch_opcode_len(const char *buf, unsigned long read);

#endif
