/*-------------------------------------------------------------------------
  gen.h - header file for code generation for the LH5801 port.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
-------------------------------------------------------------------------*/

#ifndef LH5801GEN_H
#define LH5801GEN_H 1

#include "ralloc.h"

/* Phase 1 asmop: deliberately just two cases. Every operand is either a
   compile-time literal, or lives at a fixed, directly-addressed memory
   location (a global, a non-reentrant local, or an iTemp's static spill
   location -- see ralloc.c). There is no register-resident case: A is
   used only as scratch within a single gen*() call, never as a place
   where a value is expected to still be sitting in a later iCode. */
typedef enum
{
  AOP_LIT,
  AOP_DIR
}
AOP_TYPE;

typedef struct asmop
{
  AOP_TYPE type;
  short size;
  union
    {
      value *lit;   /* AOP_LIT: the literal value */
      char *dir;    /* AOP_DIR: base address text; byte i (0 = first
                        emitted byte) lives at "dir + i" */
    }
  u;
}
asmop;

void genLH5801Code (iCode *);

#endif
