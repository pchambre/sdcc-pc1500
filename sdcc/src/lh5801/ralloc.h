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

/* Phase 1: no real register allocation. A is used only transiently by
   gen.c, one operation at a time; every value that must survive between
   iCodes (locals, iTemps, globals) lives in directly-addressed memory
   instead. See docs/lh5801_register_allocation.md (pc1500emu project
   notes) for why: unlike e.g. STM8's X/Y, LH5801's X/Y/U registers have
   no general 16-bit arithmetic (only +-1 INC/DEC), so they buy us little
   as arithmetic operand homes -- their main value is as address
   pointers, which is a later, separable optimization. */
enum
{
  A_IDX = 0, /* The accumulator */
  SP_IDX     /* S -- for use with debug info only */
};

typedef struct reg_info
{
  short type;
  short rIdx;
  const char *name;
} reg_info;

extern reg_info lh5801_regs[];

void lh5801_assignRegisters (ebbIndex *);

#endif
