#include "archs.h"
#include "ulas.h"

/**
 * Instruction table
 * for SM83
 */

// define <name> r8, r8
#define ULAS_INSTRSM83_R8R8(name, base_op, reg_left, reg_right)                \
  {                                                                            \
    (name), {(reg_left), ',', (reg_right), 0}, { base_op, 0 }                  \
  }

#define ULAS_INSTRSM83_R16R16(name, op, reg_left, reg_right)                   \
  ULAS_INSTRSM83_R8R8(name, op, reg_left, reg_right)

#define ULAS_INSTRSM83_R8R8D(name, base_op, reg_left)                          \
  ULAS_INSTRSM83_R8R8(name, base_op, reg_left, ULAS_REGSM83_B),                \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 1, reg_left, ULAS_REGSM83_C),      \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 2, reg_left, ULAS_REGSM83_D),      \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 3, reg_left, ULAS_REGSM83_E),      \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 4, reg_left, ULAS_REGSM83_H),      \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 5, reg_left, ULAS_REGSM83_L),      \
      {(name),                                                                 \
       {(reg_left), ',', '[', ULAS_REGSM83_HL, ']', 0},                        \
       {(base_op) + 6, 0}},                                                    \
      ULAS_INSTRSM83_R8R8(name, (base_op) + 7, reg_left, ULAS_REGSM83_A)

// <name> a, r8
#define ULAS_INSTRSM83_ALUR8D(name, base_op)                                   \
  ULAS_INSTRSM83_R8R8D(name, base_op, ULAS_REGSM83_A)

#define ULAS_INSTRSM83_R8_EXPR8(name, op, reg_left)                            \
  {                                                                            \
    (name), {(reg_left), ',', ULAS_E8, 0}, { (op), ULAS_E8, 0 }                \
  }

// <name> r16, e16
#define ULAS_INSTRSM83_R16E16(name, op, reg_left)                              \
  {                                                                            \
    (name), {(reg_left), ',', ULAS_E16, 0}, { (op), ULAS_E16, 0 }              \
  }

// <name> reg
#define ULAS_INSTRSM83_REG(name, op, reg)                                      \
  {                                                                            \
    (name), {(reg), 0}, { (op), 0x00 }                                         \
  }

// prefixed <name> reg
#define ULAS_INSTRSM83_PRER8(name, base_op, reg_right)                         \
  {                                                                            \
    (name), {(reg_right), 0}, { 0xCB, base_op, 0 }                             \
  }

#define ULAS_INSTRSM83_PRER8D(name, base_op)                                   \
  ULAS_INSTRSM83_PRER8(name, (base_op), ULAS_REGSM83_B),                       \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 1, ULAS_REGSM83_C),               \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 2, ULAS_REGSM83_D),               \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 3, ULAS_REGSM83_E),               \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 4, ULAS_REGSM83_H),               \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 5, ULAS_REGSM83_L),               \
      {(name), {'[', ULAS_REGSM83_HL, ']', 0}, {0xCB, (base_op) + 6, 0}},      \
      ULAS_INSTRSM83_PRER8(name, (base_op) + 7, ULAS_REGSM83_A)

// prefixed <name> <bit>, reg
#define ULAS_INSTRSM83_PREBITR8(name, base_op, bit, reg_right)                 \
  {                                                                            \
    (name), {(bit), ',', (reg_right), 0}, { 0xCB, base_op, 0 }                 \
  }

#define ULAS_INSTRSM83_PREBITR8D(name, base_op, bit)                           \
  ULAS_INSTRSM83_PREBITR8(name, base_op, bit, ULAS_REGSM83_B),                 \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 1, bit, ULAS_REGSM83_C),       \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 2, bit, ULAS_REGSM83_D),       \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 3, bit, ULAS_REGSM83_E),       \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 4, bit, ULAS_REGSM83_H),       \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 5, bit, ULAS_REGSM83_L),       \
      {(name),                                                                 \
       {(bit), ',', '[', ULAS_REGSM83_HL, ']', 0},                             \
       {0xCB, (base_op) + 6, 0}},                                              \
      ULAS_INSTRSM83_PREBITR8(name, (base_op) + 7, bit, ULAS_REGSM83_A)

// all instructions
// when name is NULL list ended
// FIXME: Add ULAS_A16 and make all absolute calls/jumps A16 instead of E16
const struct ulas_instr ULASINSTRS_SM83[] = {
    // control instructions
    {"nop", {0}, {(short)ULAS_DATZERO, 0}},
    {"halt", {0}, {0x76, 0}},
    {"stop", {0}, {0x10, (short)ULAS_DATZERO, 0x00}},
    {"di", {0}, {0xF3, 0x00}},
    {"ei", {0}, {0xFB, 0x00}},

    // misc
    {"daa", {0}, {0x27, 0x00}},
    {"scf", {0}, {0x37, 0x00}},

    {"cpl", {0}, {0x2F, 0x00}},
    {"ccf", {0}, {0x3F, 0x00}},

    // shift / bits
    {"rlca", {0}, {0x07, 0x00}},
    {"rls", {0}, {0x17, 0x00}},
    {"rrca", {0}, {0x0F, 0x00}},
    {"rra", {0}, {0x1F, 0x00}},

    // ld r8, r8
    ULAS_INSTRSM83_R8R8D("ld", 0x40, ULAS_REGSM83_B),
    ULAS_INSTRSM83_R8R8D("ld", 0x48, ULAS_REGSM83_C),
    ULAS_INSTRSM83_R8R8D("ld", 0x50, ULAS_REGSM83_D),
    ULAS_INSTRSM83_R8R8D("ld", 0x58, ULAS_REGSM83_E),
    ULAS_INSTRSM83_R8R8D("ld", 0x60, ULAS_REGSM83_H),
    ULAS_INSTRSM83_R8R8D("ld", 0x68, ULAS_REGSM83_L),
    ULAS_INSTRSM83_R8R8D("ld", 0x78, ULAS_REGSM83_A),

    // ld [r16], a
    {"ld", {'[', ULAS_REGSM83_BC, ']', ',', ULAS_REGSM83_A, 0}, {0x02, 0}},
    {"ld", {'[', ULAS_REGSM83_DE, ']', ',', ULAS_REGSM83_A, 0}, {0x12, 0}},
    {"ld", {'[', ULAS_REGSM83_HL, ']', ',', ULAS_REGSM83_A, 0}, {0x77, 0}},
    {"ld", {'[', ULAS_REGSM83_HL, '+', ']', ',', ULAS_REGSM83_A, 0}, {0x22, 0}},
    {"ld", {'[', ULAS_REGSM83_HL, '-', ']', ',', ULAS_REGSM83_A, 0}, {0x32, 0}},
    {"ld", {'[', ULAS_REGSM83_HL, ']', ',', ULAS_E8, 0}, {0x36, ULAS_E8, 0x00}},

    // ld a, [r16]
    {"ld", {ULAS_REGSM83_A, ',', '[', ULAS_REGSM83_BC, ']', 0}, {0x0A, 0}},
    {"ld", {ULAS_REGSM83_A, ',', '[', ULAS_REGSM83_DE, ']', 0}, {0x1A, 0}},
    {"ld", {ULAS_REGSM83_A, ',', '[', ULAS_REGSM83_HL, '+', ']', 0}, {0x2A, 0}},
    {"ld", {ULAS_REGSM83_A, ',', '[', ULAS_REGSM83_HL, '-', ']', 0}, {0x3A, 0}},

    {"ld", {'[', ULAS_E16, ']', ',', ULAS_REGSM83_SP, 0}, {0x08, ULAS_E16, 0}},

    {"ld", {'[', ULAS_E16, ']', ',', ULAS_REGSM83_A, 0}, {0xEA, ULAS_E16, 0}},
    {"ld", {ULAS_REGSM83_A, ',', '[', ULAS_E16, ']', 0}, {0xFA, ULAS_E16, 0}},

    {"ld",
     {ULAS_REGSM83_HL, ',', ULAS_REGSM83_SP, '+', ULAS_E8, 0},
     {0xF8, ULAS_E8, 0}},

    {"ldh", {'[', ULAS_REGSM83_C, ']', ',', ULAS_REGSM83_A, 0}, {0xE2, 0}},
    {"ldh", {ULAS_REGSM83_A, ',', '[', ULAS_REGSM83_C, ']', 0}, {0xF2, 0}},

    {"ldh", {'[', ULAS_A8, ']', ',', ULAS_REGSM83_A, 0}, {0xE0, ULAS_A8, 0}},
    {"ldh", {ULAS_REGSM83_A, ',', '[', ULAS_A8, ']', 0}, {0xF0, ULAS_A8, 0}},

    // ld r8, e8
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x06, ULAS_REGSM83_B),
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x16, ULAS_REGSM83_D),
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x26, ULAS_REGSM83_H),

    ULAS_INSTRSM83_R8_EXPR8("ld", 0x0E, ULAS_REGSM83_C),
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x1E, ULAS_REGSM83_E),
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x2E, ULAS_REGSM83_L),
    ULAS_INSTRSM83_R8_EXPR8("ld", 0x3E, ULAS_REGSM83_A),

    // ld r16, e16
    ULAS_INSTRSM83_R16E16("ld", 0x01, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_R16E16("ld", 0x11, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_R16E16("ld", 0x21, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_R16E16("ld", 0x31, ULAS_REGSM83_SP),

    // jr
    ULAS_INSTRSM83_R8_EXPR8("jr", 0x20, ULAS_REGSM83_NOT_ZERO),
    ULAS_INSTRSM83_R8_EXPR8("jr", 0x30, ULAS_REGSM83_NOT_CARRY),
    ULAS_INSTRSM83_R8_EXPR8("jr", 0x28, ULAS_REGSM83_ZERO),
    ULAS_INSTRSM83_R8_EXPR8("jr", 0x38, ULAS_REGSM83_CARRY),
    {"jr", {ULAS_E8, 0}, {0x18, ULAS_E8, 0x00}},

    // ret
    ULAS_INSTRSM83_REG("ret", 0xC0, ULAS_REGSM83_NOT_ZERO),
    ULAS_INSTRSM83_REG("ret", 0xD0, ULAS_REGSM83_NOT_CARRY),
    ULAS_INSTRSM83_REG("ret", 0xC8, ULAS_REGSM83_ZERO),
    ULAS_INSTRSM83_REG("ret", 0xD8, ULAS_REGSM83_CARRY),
    {"ret", {0}, {0xC9, 0x00}},
    {"reti", {0}, {0xD9, 0x00}},

    // jp
    ULAS_INSTRSM83_R16E16("jp", 0xC2, ULAS_REGSM83_NOT_ZERO),
    ULAS_INSTRSM83_R16E16("jp", 0xD2, ULAS_REGSM83_NOT_CARRY),
    ULAS_INSTRSM83_R16E16("jp", 0xCA, ULAS_REGSM83_ZERO),
    ULAS_INSTRSM83_R16E16("jp", 0xDA, ULAS_REGSM83_CARRY),
    {"jp", {ULAS_REGSM83_HL, 0}, {0xE9, 0x00}},
    {"jp", {ULAS_E16, 0}, {0xC3, ULAS_E16, 0x00}},

    // call
    ULAS_INSTRSM83_R16E16("call", 0xC4, ULAS_REGSM83_NOT_ZERO),
    ULAS_INSTRSM83_R16E16("call", 0xD4, ULAS_REGSM83_NOT_CARRY),
    ULAS_INSTRSM83_R16E16("call", 0xCC, ULAS_REGSM83_ZERO),
    ULAS_INSTRSM83_R16E16("call", 0xDC, ULAS_REGSM83_CARRY),
    {"call", {ULAS_E16, 0}, {0xCD, ULAS_E16, 0x00}},

    // rst
    ULAS_INSTRSM83_REG("rst", 0xC7, ULAS_VECSM83_00),
    ULAS_INSTRSM83_REG("rst", 0xD7, ULAS_VECSM83_10),
    ULAS_INSTRSM83_REG("rst", 0xE7, ULAS_VECSM83_20),
    ULAS_INSTRSM83_REG("rst", 0xF7, ULAS_VECSM83_30),
    ULAS_INSTRSM83_REG("rst", 0xCF, ULAS_VECSM83_08),
    ULAS_INSTRSM83_REG("rst", 0xDF, ULAS_VECSM83_18),
    ULAS_INSTRSM83_REG("rst", 0xEF, ULAS_VECSM83_28),
    ULAS_INSTRSM83_REG("rst", 0xFF, ULAS_VECSM83_38),

    // inc/dec
    ULAS_INSTRSM83_REG("inc", 0x03, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_REG("inc", 0x13, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_REG("inc", 0x23, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_REG("inc", 0x33, ULAS_REGSM83_SP),

    ULAS_INSTRSM83_REG("inc", 0x04, ULAS_REGSM83_B),
    ULAS_INSTRSM83_REG("inc", 0x14, ULAS_REGSM83_D),
    ULAS_INSTRSM83_REG("inc", 0x24, ULAS_REGSM83_H),
    {"inc", {'[', ULAS_REGSM83_HL, ']', 0}, {0x34, 0x00}},

    ULAS_INSTRSM83_REG("dec", 0x05, ULAS_REGSM83_B),
    ULAS_INSTRSM83_REG("dec", 0x15, ULAS_REGSM83_D),
    ULAS_INSTRSM83_REG("dec", 0x25, ULAS_REGSM83_H),
    {"dec", {'[', ULAS_REGSM83_HL, ']', 0}, {0x35, 0x00}},

    ULAS_INSTRSM83_REG("dec", 0x0B, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_REG("dec", 0x1B, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_REG("dec", 0x2B, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_REG("dec", 0x3B, ULAS_REGSM83_SP),

    ULAS_INSTRSM83_REG("inc", 0x0C, ULAS_REGSM83_C),
    ULAS_INSTRSM83_REG("inc", 0x1C, ULAS_REGSM83_E),
    ULAS_INSTRSM83_REG("inc", 0x2C, ULAS_REGSM83_L),
    ULAS_INSTRSM83_REG("inc", 0x3C, ULAS_REGSM83_A),

    ULAS_INSTRSM83_REG("dec", 0x0D, ULAS_REGSM83_C),
    ULAS_INSTRSM83_REG("dec", 0x1D, ULAS_REGSM83_E),
    ULAS_INSTRSM83_REG("dec", 0x2D, ULAS_REGSM83_L),
    ULAS_INSTRSM83_REG("dec", 0x3D, ULAS_REGSM83_A),

    // alu r8, r8
    ULAS_INSTRSM83_ALUR8D("add", 0x80),
    ULAS_INSTRSM83_ALUR8D("adc", 0x88),
    ULAS_INSTRSM83_ALUR8D("sub", 0x90),
    ULAS_INSTRSM83_ALUR8D("sbc", 0x98),
    ULAS_INSTRSM83_ALUR8D("and", 0xA0),
    ULAS_INSTRSM83_ALUR8D("xor", 0xA8),
    ULAS_INSTRSM83_ALUR8D("or", 0xB0),
    ULAS_INSTRSM83_ALUR8D("cp", 0xB8),

    ULAS_INSTRSM83_R8_EXPR8("add", 0xC6, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("sub", 0xD6, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("and", 0xE6, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("or", 0xF6, ULAS_REGSM83_A),

    ULAS_INSTRSM83_R8_EXPR8("adc", 0xCE, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("suc", 0xDE, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("xor", 0xEE, ULAS_REGSM83_A),
    ULAS_INSTRSM83_R8_EXPR8("cp", 0xFE, ULAS_REGSM83_A),

    ULAS_INSTRSM83_R8_EXPR8("add", 0xE8, ULAS_REGSM83_SP),

    // alu r16, r16
    ULAS_INSTRSM83_R16R16("add", 0x09, ULAS_REGSM83_HL, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_R16R16("add", 0x19, ULAS_REGSM83_HL, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_R16R16("add", 0x29, ULAS_REGSM83_HL, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_R16R16("add", 0x39, ULAS_REGSM83_HL, ULAS_REGSM83_SP),

    // pop
    ULAS_INSTRSM83_REG("pop", 0xC1, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_REG("pop", 0xD1, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_REG("pop", 0xE1, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_REG("pop", 0xF1, ULAS_REGSM83_AF),

    // push
    ULAS_INSTRSM83_REG("push", 0xC5, ULAS_REGSM83_BC),
    ULAS_INSTRSM83_REG("push", 0xD5, ULAS_REGSM83_DE),
    ULAS_INSTRSM83_REG("push", 0xE5, ULAS_REGSM83_HL),
    ULAS_INSTRSM83_REG("push", 0xF5, ULAS_REGSM83_AF),

    // prefixed
    ULAS_INSTRSM83_PRER8D("swap", 0x30),
    ULAS_INSTRSM83_PRER8D("rlc", 0x00),
    ULAS_INSTRSM83_PRER8D("rrc", 0x08),
    ULAS_INSTRSM83_PRER8D("rl", 0x10),
    ULAS_INSTRSM83_PRER8D("rr", 0x18),
    ULAS_INSTRSM83_PRER8D("sla", 0x10),
    ULAS_INSTRSM83_PRER8D("sra", 0x18),
    ULAS_INSTRSM83_PRER8D("srl", 0x38),

    ULAS_INSTRSM83_PREBITR8D("bit", 0x40, '0'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x48, '1'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x50, '2'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x58, '3'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x60, '4'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x68, '5'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x70, '6'),
    ULAS_INSTRSM83_PREBITR8D("bit", 0x78, '7'),

    ULAS_INSTRSM83_PREBITR8D("res", 0x80, '0'),
    ULAS_INSTRSM83_PREBITR8D("res", 0x88, '1'),
    ULAS_INSTRSM83_PREBITR8D("res", 0x90, '2'),
    ULAS_INSTRSM83_PREBITR8D("res", 0x98, '3'),
    ULAS_INSTRSM83_PREBITR8D("res", 0xA0, '4'),
    ULAS_INSTRSM83_PREBITR8D("res", 0xA8, '5'),
    ULAS_INSTRSM83_PREBITR8D("res", 0xB0, '6'),
    ULAS_INSTRSM83_PREBITR8D("res", 0xB8, '7'),

    ULAS_INSTRSM83_PREBITR8D("set", 0xC0, '0'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xC8, '1'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xD0, '2'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xD8, '3'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xE0, '4'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xE8, '5'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xF0, '6'),
    ULAS_INSTRSM83_PREBITR8D("set", 0xF8, '7'),

    {NULL}};

const char *ULAS_SM83_REGS[] = {
    NULL,   "b",    "c",    "d",    "e",    "h",    "l",   "a",  "bc",
    "de",   "hl",   "nz",   "z",    "nc",   "c",    "sp",  "af", "0x00",
    "0x08", "0x10", "0x18", "0x20", "0x28", "0x30", "0x38"};

void ulas_arch_set(enum ulas_archs arch) {
  switch (arch) {
  case ULAS_ARCH_SM83:
    ulas.arch = (struct ulas_arch){ULAS_SM83_REGS, ULAS_SM83_REGS_LEN,
                                   ULASINSTRS_SM83, ULAS_LE};
    break;
  default:
    ULASPANIC("Unknown architecture\n");
  }
}
