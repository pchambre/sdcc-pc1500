/*-------------------------------------------------------------------------
  ralloc.h - register allocation for the LH5801 port.

  Paul Chambre, 2026

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
-------------------------------------------------------------------------*/

#ifndef LH5801RALLOC_H
#define LH5801RALLOC_H 1

#include "common.h"

/* The 8-bit registers a value byte can live in: A, and the halves of the
   X/Y/U pointer registers. With the old allocator (the default for now,
   see main.c's --newralloc) nothing lives in them: every value that must
   survive between iCodes is in directly-addressed memory, and A and X are
   gen.c's scratch. The new one (ralloc2.cc, SDCC's tree-decomposition
   allocator) puts bytes in the registers lh5801_reg_mask allows. Unlike
   e.g. STM8's X/Y, the LH5801's X/Y/U have no general 16-bit arithmetic
   (only +-1 INC/DEC), so as byte homes they're used through A: LDA/STA/
   ADC/SBC/CPA take a half as a 1-byte operand. */
enum
{
  LH5801_REG_A = 0,
  LH5801_REG_XL,
  LH5801_REG_XH,
  LH5801_REG_YL,
  LH5801_REG_YH,
  LH5801_REG_UL,
  LH5801_REG_UH,
  LH5801_NUM_REGS,               /* = port->num_regs */
  SP_IDX = LH5801_NUM_REGS       /* S -- for use with debug info only */
};

/* Register/live-range types, as in hc08: sym->regType, reg_info.type */
enum
{
  REG_PTR = 1,
  REG_GPR,
  REG_CND
};

typedef struct reg_info
{
  short type;
  short rIdx;
  const char *name;
} reg_info;

extern reg_info lh5801_regs[];

/* --newralloc: use the tree-decomposition allocator (ralloc2.cc). */
extern bool lh5801_newralloc;
/* Which registers it may use, one bit per LH5801_REG_* -- see main.c. */
extern unsigned int lh5801_reg_mask;

void lh5801_assignRegisters (ebbIndex *);
iCode *lh5801_ralloc2_cc (ebbIndex *ebbi);

#endif
