#ifndef ULDAS_H_
#define ULDAS_H_

/**
 * Disassembly module
 */
#include "ulas.h"
#include "archs.h"

// Disassemble a file based on the current arch
int ulas_dasm(FILE *src, FILE *dst);

#endif
