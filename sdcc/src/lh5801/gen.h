/*-------------------------------------------------------------------------
  gen.h - header file for code generation for the LH5801 port.

  Paul Chambre, 2026

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
-------------------------------------------------------------------------*/

#ifndef LH5801GEN_H
#define LH5801GEN_H 1

#include "ralloc.h"

/* Phase 1 asmop: deliberately just three cases. Every operand is either
   a compile-time literal, lives at a fixed, directly-addressed memory
   location (a global, a non-reentrant local, or an iTemp's static spill
   location -- see ralloc.c), or is a bare reference to a function (a
   function used as a value, not called -- decays to its address exactly
   like an array does, e.g. `pfn_outputchar p = put_char_to_stdout;`).
   That last case needs its own AOP kind: a function symbol's own rname
   is a CODE-space label, not a memory location holding a value, so it
   must be read via ">sym"/"<sym" (a link-time constant address, exactly
   like genAddrOf()'s own pattern), never dereferenced with lda/sta like
   AOP_DIR -- confirmed directly: treating it as AOP_DIR compiled
   `output_char = put_char_to_stdout;` (vprintf.c) into "lda
   (put_char_to_stdout)", reading the function's own *machine code* as
   if it were a stored pointer value, corrupting the PCALL trampoline's
   patched target and sending execution into never-mapped memory.
   AOP_REG is for the register allocator: until it assigns registers,
   nothing is register-resident and A is only scratch within one gen*()
   call. Every gen*() reaches operand bytes through gen.c's loadA()/
   storeA()/opA(), which know each operand type, so AOP_REG needs no
   further changes there. */
typedef enum
{
  AOP_LIT,
  AOP_DIR,
  AOP_FUNC,
  /* A rematerializable iTemp (ralloc.c's markRemat()) -- one whose only
     value is a link-time-constant address, "&sym + k" (an array element
     or struct member of a global or static, through any number of
     constant additions and pointer casts). It has no storage at all: a
     pointer load or store through it is a plain direct "lda/sta (sym +
     k)", any other use of its value is ">(sym + k)"/"<(sym + k)", and the
     iCodes that only computed it emit nothing. */
  AOP_IMMD,
  /* In registers: byte i lives in regs[i], one of the LH5801_REG_* below
     (the register allocator's choice -- see ralloc2.cc). */
  AOP_REG
}
AOP_TYPE;

/* The 8-bit registers a value byte can live in: A, and the halves of the
   X/Y/U pointer registers. Indices into gen.c's register-name table. */
enum
{
  LH5801_REG_A = 0,
  LH5801_REG_XL,
  LH5801_REG_XH,
  LH5801_REG_YL,
  LH5801_REG_YH,
  LH5801_REG_UL,
  LH5801_REG_UH,
  LH5801_NUM_REGS
};

typedef struct asmop
{
  AOP_TYPE type;
  short size;
  union
    {
      value *lit;   /* AOP_LIT: the literal value */
      char *dir;    /* AOP_DIR: base address text; byte i (0 = first
                        emitted byte) lives at "dir + i".
                        AOP_FUNC: the function's own rname, read via
                        ">dir"/"<dir" instead of "(dir + i)".
                        AOP_IMMD: the address expression, "(sym + k)". */
      signed char regs[8];  /* AOP_REG: byte i's register */
    }
  u;
}
asmop;

void genLH5801Code (iCode *);

/* Emits a bare label definition for any basic block SDCCBBlock.c's
   iCodeFromeBBlock() silently dropped from genLH5801Code()'s own linear
   chain -- see ralloc.c's caller for why this is needed at all. */
void genLH5801OrphanedLabels (eBBlock **ebbs, int count);

/* Port hook (main.c's PORT struct's genAssemblerStart): emits this
   port's one-off static scratch/frame-pointer locations, unconditionally,
   before any function's code -- see the function's own comment (gen.c)
   for why they can never be emitted lazily, on first use, from inside
   gen.c's normal per-function codegen path. */
void genLH5801AssemblerStart (FILE *of);

#endif
