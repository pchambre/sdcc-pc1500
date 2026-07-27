/*-------------------------------------------------------------------------
  gen.c - code generator for the LH5801 port.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  Phase 1: deliberately minimal. There is no register allocation (see
  ralloc.c) -- every operand is either a compile-time literal or lives at
  a fixed, directly-addressed memory location. A is used only as scratch
  within a single gen*() call. Only enough iCode operators are handled to
  prove the compile -> sdas/aslh5801 -> sdld -> pc1500emu pipeline works
  end to end; everything else fails loudly via wassertl(0, ...) rather
  than silently miscompiling.
-------------------------------------------------------------------------*/

#include "ralloc.h"
#include "gen.h"
#include "SDCCgen.h"

static void
aopOp (operand *op)
{
  asmop *aop = (asmop *) Safe_alloc (sizeof (asmop));

  op->aop = aop;
  aop->size = getSize (operandType (op));

  if (IS_OP_LITERAL (op))
    {
      aop->type = AOP_LIT;
      aop->u.lit = OP_VALUE (op);
      return;
    }

  wassertl (IS_SYMOP (op), "lh5801: operand is neither a literal nor a symbol");

  {
    symbol *sym = OP_SYMBOL (op);

    if (sym->isspilt && sym->usl.spillLoc)
      sym = sym->usl.spillLoc;

    wassertl (sym->rname[0],
      "lh5801: symbol has no assigned memory address -- the phase-1 backend "
      "only supports memory-resident operands (no register allocation yet)");

    aop->type = AOP_DIR;
    aop->u.dir = sym->rname;
  }
}

static void
freeAsmop (operand *op)
{
  if (op && op->aop)
    {
      Safe_free (op->aop);
      op->aop = NULL;
    }
}

/* Address text for the byte at "offset" (0 = first emitted byte). */
static void
dirAddr (char *buf, size_t bufsz, const asmop *aop, int offset)
{
  wassertl (aop->type == AOP_DIR, "lh5801: dirAddr() called on a non-AOP_DIR operand");

  if (offset == 0)
    snprintf (buf, bufsz, "%s", aop->u.dir);
  else
    snprintf (buf, bufsz, "%s + %d", aop->u.dir, offset);
}

/* Value of the byte at "offset" (0 = first emitted byte) of an AOP_LIT
   operand being stored/interpreted as "size" bytes, big endian (LH5801 is
   big endian -- see main.c's little_endian = false and the byte order
   confirmed for SJP/STA absolute operands in the sdas/aslh5801 linker
   test). byteOfVal()'s own index is little-endian (0 = least
   significant), so byte 0 of our big-endian layout is byteOfVal's
   most-significant byte. */
static unsigned char
litByte (const asmop *aop, int offset, int size)
{
  wassertl (aop->type == AOP_LIT, "lh5801: litByte() called on a non-AOP_LIT operand");
  return byteOfVal (aop->u.lit, size - 1 - offset);
}

/*-----------------------------------------------------------------*/
/* genFunction - generate the prologue (just a label, for now)     */
/*-----------------------------------------------------------------*/
static void
genFunction (const iCode *ic)
{
  symbol *sym = OP_SYMBOL (IC_LEFT (ic));

  emitcode (";", "-----------------------------------------");
  emitcode (";", " function %s", sym->name);
  emitcode (";", "-----------------------------------------");

  emitcode ("", "%s:", sym->rname);
  genLine.lineCurr->isLabel = 1;
}

/*-----------------------------------------------------------------*/
/* genEndFunction - generate the epilogue                          */
/*-----------------------------------------------------------------*/
static void
genEndFunction (const iCode *ic)
{
  (void) ic;
  emitcode ("rtn", "");
}

/*-----------------------------------------------------------------*/
/* genReturn - generate code for a return statement                */
/*-----------------------------------------------------------------*/
static void
genReturn (const iCode *ic)
{
  operand *left = IC_LEFT (ic);

  if (left)
    {
      aopOp (left);

      wassertl (left->aop->size <= 1,
        "lh5801: phase-1 backend only supports 0- or 1-byte return values");

      if (left->aop->size == 1)
        {
          if (left->aop->type == AOP_LIT)
            emitcode ("ldi", "a, 0x%02x", litByte (left->aop, 0, 1));
          else
            emitcode ("lda", "(%s)", left->aop->u.dir);
        }

      freeAsmop (left);
    }

  if (!(ic->next && ic->next->op == LABEL && IC_LABEL (ic->next) == returnLabel))
    emitcode ("bch", "!tlabel", labelKey2num (returnLabel->key));
}

/*-----------------------------------------------------------------*/
/* genLabel - generate a label                                     */
/*-----------------------------------------------------------------*/
static void
genLabel (const iCode *ic)
{
  if (IC_LABEL (ic) == entryLabel)
    return;

  emitLabel (IC_LABEL (ic));
}

/*-----------------------------------------------------------------*/
/* genGoto - generate an unconditional jump                        */
/*-----------------------------------------------------------------*/
static void
genGoto (const iCode *ic)
{
  emitcode ("bch", "!tlabel", labelKey2num (IC_LABEL (ic)->key));
}

/*-----------------------------------------------------------------*/
/* genAssign - generate code for '='                                */
/*-----------------------------------------------------------------*/
static void
genAssign (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  int size;
  int i;

  aopOp (right);
  aopOp (result);

  size = result->aop->size;

  for (i = 0; i < size; i++)
    {
      char buf[128];

      if (right->aop->type == AOP_LIT)
        emitcode ("ldi", "a, 0x%02x", litByte (right->aop, i, size));
      else
        {
          dirAddr (buf, sizeof (buf), right->aop, i);
          emitcode ("lda", "(%s)", buf);
        }

      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
    }

  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genLH5801Code - generate code for a linear iCode chain          */
/*-----------------------------------------------------------------*/
void
genLH5801Code (iCode *lic)
{
  iCode *ic;

  for (ic = lic; ic; ic = ic->next)
    {
      initGenLineElement ();
      genLine.lineElement.ic = ic;

      if (ic->generated)
        continue;

      switch (ic->op)
        {
        case FUNCTION:
          genFunction (ic);
          break;
        case ENDFUNCTION:
          genEndFunction (ic);
          break;
        case RETURN:
          genReturn (ic);
          break;
        case LABEL:
          genLabel (ic);
          break;
        case GOTO:
          genGoto (ic);
          break;
        case '=':
          genAssign (ic);
          break;
        default:
          wassertl (0, "lh5801: iCode operator not yet implemented in the phase-1 backend");
          break;
        }
    }

  /* Flush the accumulated line list into the output buffer -- every other
     port's gen*Code() does this as its last act (e.g. stm8/gen.c,
     mcs51/gen.c); nothing shared calls printLine() for us. */
  printLine (genLine.lineHead, codeOutBuf);
}
