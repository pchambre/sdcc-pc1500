/*-------------------------------------------------------------------------
  gen.c - code generator for the LH5801 port.

  Paul Chambre, 2026

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

/*-----------------------------------------------------------------*/
/* Output, and the register allocator's dry run                     */
/*                                                                   */
/* Every line gen.c produces goes through emit()/emitLbl(). When the  */
/* tree-decomposition allocator (ralloc2.cc) prices a candidate       */
/* register assignment it runs the code generator for one iCode with  */
/* regalloc_dry_run set (drylh5801iCode()): then nothing is output and */
/* each line only adds its size in bytes to dryCost -- so the          */
/* allocator optimizes exactly what this file would emit.              */
/*-----------------------------------------------------------------*/

extern bool regalloc_dry_run;   /* SDCCmain.c; SDCCicode.c reads it too */
static float dryCost;

/* The size in bytes of one emitted line, "mnemonic<TAB>operands" --
   by operand form, as sdas/aslh5801/m5801mch.c encodes them. */
static int
instSize (const char *line)
{
  char mn[16];
  const char *ops;
  size_t n = strcspn (line, "\t ");

  if (n >= sizeof (mn))
    n = sizeof (mn) - 1;
  memcpy (mn, line, n);
  mn[n] = 0;
  ops = line + n;
  while (*ops == '\t' || *ops == ' ')
    ops++;

  if (!strcmp (mn, "jmp") || !strcmp (mn, "sjp"))
    return 3;
  if (mn[0] == 'b' && strcmp (mn, "bii") || !strcmp (mn, "lop") || !strcmp (mn, "vmj"))
    return 2;                         /* relative branches, LOP, VMJ */
  if (!strcmp (mn, "psh") || !strcmp (mn, "pop") || !strcmp (mn, "ldx") || !strcmp (mn, "stx") ||
      !strcmp (mn, "sie") || !strcmp (mn, "rie") || !strcmp (mn, "hlt") || !strcmp (mn, "am0") ||
      !strcmp (mn, "am1") || !strcmp (mn, "sdp") || !strcmp (mn, "rdp"))
    return 2;                         /* FD-prefixed */
  if ((!strcmp (mn, "inc") || !strcmp (mn, "dec")) && ops[0] && ops[1] == 'h')
    return 2;                         /* INC/DEC XH/YH/UH: FD-prefixed */
  if (ops[0] == '#' && ops[1] == '(')
    return strstr (ops, "),") ? 5 : 4;  /* ME1 (I/O space), FD-prefixed */
  if (ops[0] == '(' && ops[2] != ')')
    return strstr (ops, "),") ? 4 : 3;  /* (ab) or (ab),i -- not (x)/(y)/(u) */
  if (!strcmp (mn, "ldi") || strchr (ops, '#'))
    return 2;                         /* an immediate byte */
  return 1;                           /* inherent, register, (x)/(y)/(u) */
}

static void
emit (const char *inst, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  if (regalloc_dry_run)
    {
      const char *line = format_opcode (inst, fmt, ap);

      if (inst && *inst)
        dryCost += instSize (line);
      dbuf_free (line);
    }
  else
    va_emitcode (inst, fmt, ap);
  va_end (ap);
}

static void
emitLbl (const symbol *tlbl)
{
  if (!regalloc_dry_run)
    emitLabel (tlbl);
}

/* The constant address a rematerializable iTemp (ralloc.c's markRemat())
   stands for: its symbol's rname, plus the sum of every constant offset
   along the chain of iCodes that computed it. */
static const char *
rematBase (const symbol *sym, long *offset)
{
  const iCode *ic = sym->rematiCode;

  switch (ic->op)
    {
    case ADDRESS_OF:
      if (IC_RIGHT (ic))
        *offset += (long) operandLitValue (IC_RIGHT (ic));
      return OP_SYMBOL (IC_LEFT (ic))->rname;
    case '+':
      *offset += (long) operandLitValue (IC_RIGHT (ic));
      return rematBase (OP_SYMBOL (IC_LEFT (ic)), offset);
    case '-':
      *offset -= (long) operandLitValue (IC_RIGHT (ic));
      return rematBase (OP_SYMBOL (IC_LEFT (ic)), offset);
    default:  /* CAST, '=' */
      return rematBase (OP_SYMBOL (IC_RIGHT (ic)), offset);
    }
}

static bool
isRemat (const operand *op)
{
  return op && IS_ITEMP (op) && OP_SYMBOL_CONST (op)->remat;
}

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

  if (isRemat (op))
    {
      long offset = 0;
      const char *base = rematBase (OP_SYMBOL (op), &offset);
      char buf[800 + 32];   /* rname can be up to SDCC_NAME_MAX long, as in genAddrOf() */

      if (offset)
        snprintf (buf, sizeof (buf), "(%s %c 0x%lx)", base, offset < 0 ? '-' : '+',
                  (unsigned long) (offset < 0 ? -offset : offset));
      else
        snprintf (buf, sizeof (buf), "(%s)", base);
      aop->type = AOP_IMMD;
      aop->u.dir = Safe_strdup (buf);
      return;
    }

  {
    symbol *sym = OP_SYMBOL (op);

    /* In registers (the new allocator's choice -- ralloc2.cc). */
    if (sym->isitmp && !sym->isspilt && sym->nRegs && sym->regs[0])
      {
        int i;

        aop->type = AOP_REG;
        for (i = 0; i < aop->size && i < 8; i++)
          {
            wassertl (sym->regs[i], "lh5801: operand only partly in registers");
            aop->u.regs[i] = sym->regs[i]->rIdx;
          }
        return;
      }

    if (sym->isspilt && sym->usl.spillLoc)
      sym = sym->usl.spillLoc;

    /* A dry run (the allocator pricing an assignment) meets temps that
       will be in memory but don't have their spill location yet: any
       direct address costs the same. */
    if (regalloc_dry_run && sym->isitmp && !sym->rname[0])
      {
        aop->type = AOP_DIR;
        aop->u.dir = (char *) "__lh5801_dry";
        return;
      }

    wassertl (sym->rname[0],
      "lh5801: symbol has no assigned memory address -- the phase-1 backend "
      "only supports memory-resident operands (no register allocation yet)");

    /* A bare function reference (used as a value, not called -- decays
       to its address exactly like an array does) -- see AOP_FUNC's own
       comment (gen.h) for why this can't be plain AOP_DIR. */
    if (IS_FUNC (sym->type))
      {
        aop->type = AOP_FUNC;
        aop->u.dir = sym->rname;
        return;
      }

    aop->type = AOP_DIR;
    aop->u.dir = sym->rname;
  }
}

static void
freeAsmop (operand *op)
{
  if (op && op->aop)
    {
      if (op->aop->type == AOP_IMMD)
        Safe_free (op->aop->u.dir);
      Safe_free (op->aop);
      op->aop = NULL;
    }
}

/* For the few places that need a memory operand (ADC/SBC/ORA/... have no
   immediate-with-link-time-expression form here): stores an AOP_IMMD
   operand's address into scratch location `slot` (0 or 1, so two such
   operands of one iCode don't collide -- declared in
   genLH5801AssemblerStart()) and turns it into an AOP_DIR operand there.
   Call right after aopOp(), before emitting anything else for the iCode. */
static void
aopToDir (operand *op, int slot)
{
  asmop *aop = op->aop;

  if (aop->type != AOP_IMMD)
    return;
  emit ("ldi", "a, >%s", aop->u.dir);
  emit ("sta", "(__lh5801_remat%d)", slot);
  emit ("ldi", "a, <%s", aop->u.dir);
  emit ("sta", "(__lh5801_remat%d + 1)", slot);
  Safe_free (aop->u.dir);
  aop->type = AOP_DIR;
  aop->u.dir = (char *) (slot ? "__lh5801_remat1" : "__lh5801_remat0");
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
/* Operand byte access -- every gen*() reaches its operands' bytes    */
/* through these three, so each one knows every operand type in one   */
/* place: a literal, a memory location (AOP_DIR), a link-time-constant */
/* address (AOP_FUNC/AOP_IMMD) or a register (AOP_REG).                */
/*-----------------------------------------------------------------*/

static const char *const regName[LH5801_NUM_REGS] = {"a", "xl", "xh", "yl", "yh", "ul", "uh"};

/* The register byte `offset` of `aop` is in, or -1 if it isn't in one. */
static int
aopReg (const asmop *aop, int offset)
{
  return aop->type == AOP_REG ? aop->u.regs[offset] : -1;
}

/* A = byte `offset` of `aop` (as a `size`-byte value -- a literal's own
   size can be smaller). AOP_FUNC/AOP_IMMD are 2-byte addresses: offset
   0 is the high byte (">"), offset 1 the low byte ("<"), genAddrOf()'s
   convention. A byte already in A costs nothing. */
static void
loadA (const asmop *aop, int offset, int size)
{
  int r = aopReg (aop, offset);

  if (aop->type == AOP_LIT)
    emit ("ldi", "a, 0x%02x", litByte (aop, offset, size));
  else if (aop->type == AOP_FUNC || aop->type == AOP_IMMD)
    {
      wassertl (size == 2, "lh5801: a function reference or constant address must be a 2-byte pointer");
      emit ("ldi", "a, %s%s", offset == 0 ? ">" : "<", aop->u.dir);
    }
  else if (r >= 0)
    {
      if (r != LH5801_REG_A)
        emit ("lda", "%s", regName[r]);
    }
  else
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), aop, offset);
      emit ("lda", "(%s)", buf);
    }
}

/* Byte `offset` of `aop` = A. */
static void
storeA (const asmop *aop, int offset)
{
  int r = aopReg (aop, offset);

  if (r >= 0)
    {
      if (r != LH5801_REG_A)
        emit ("sta", "%s", regName[r]);
    }
  else
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), aop, offset);
      emit ("sta", "(%s)", buf);
    }
}

/* A = A <op> byte `offset` of `aop`: memMn is the memory/register form
   (adc, sbc, and, ora, eor, cpa), immMn the immediate one (adi, sbi,
   ani, ori, eai, cpi) for a literal. Only ADC, SBC and CPA (like LDA/
   STA) take a register half as their operand -- AND/ORA/EOR have no such
   form. */
static void
opA (const char *memMn, const char *immMn, const asmop *aop, int offset, int size)
{
  int r = aopReg (aop, offset);

  if (aop->type == AOP_LIT)
    emit (immMn, "a, #0x%02x", litByte (aop, offset, size));
  else if (r >= 0)
    {
      wassertl (r != LH5801_REG_A, "lh5801: opA() with A as both operands");
      if (!strcmp (memMn, "adc") || !strcmp (memMn, "sbc") || !strcmp (memMn, "cpa"))
        emit (memMn, "%s", regName[r]);
      else
        {
          /* AND/ORA/EOR have no register-half operand -- but they're
             commutative, so A goes to scratch memory, the register into
             A, and the scratch is the operand. Z is set from the result
             either way; they don't touch the carry. */
          wassertl (!strcmp (memMn, "and") || !strcmp (memMn, "ora") || !strcmp (memMn, "eor"),
                    "lh5801: this operation has no register-operand form");
          emit ("sta", "(__lh5801_op_scratch)");
          emit ("lda", "%s", regName[r]);
          emit (memMn, "(__lh5801_op_scratch)");
        }
    }
  else
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), aop, offset);
      emit (memMn, "(%s)", buf);
    }
}

/* The pointer register, 'x', 'y' or 'u', a 2-byte operand fills exactly
   (high byte in the H half, low byte in the L half), or 0. */
static char
regPair (const asmop *aop)
{
  if (aop->type != AOP_REG || aop->size != 2)
    return 0;
  if (aop->u.regs[0] == LH5801_REG_XH && aop->u.regs[1] == LH5801_REG_XL)
    return 'x';
  if (aop->u.regs[0] == LH5801_REG_YH && aop->u.regs[1] == LH5801_REG_YL)
    return 'y';
  if (aop->u.regs[0] == LH5801_REG_UH && aop->u.regs[1] == LH5801_REG_UL)
    return 'u';
  return 0;
}

/* Does any byte of `aop` live in the pointer register `pair` ('x'/'y'/'u')? */
static bool
aopUsesPair (const asmop *aop, char pair)
{
  int lo = pair == 'x' ? LH5801_REG_XL : pair == 'y' ? LH5801_REG_YL : LH5801_REG_UL;
  int i;

  if (aop->type != AOP_REG)
    return false;
  for (i = 0; i < aop->size; i++)
    if (aop->u.regs[i] == lo || aop->u.regs[i] == lo + 1)
      return true;
  return false;
}

/* Is the value in register operand `aop` still live after `ic`
   (ralloc2.cc's set_surviving_regs())? */
static bool
aopSurvives (const iCode *ic, const asmop *aop)
{
  int i;

  if (aop->type != AOP_REG || !ic->rSurv)
    return false;
  for (i = 0; i < aop->size; i++)
    if (bitVectBitValue (ic->rSurv, aop->u.regs[i]))
      return true;
  return false;
}

/* X = the 2-byte pointer `aop` + a constant `offset`: a literal address
   straight into X, anything else low byte then high byte through A, the
   offset added on the way (ADI's carry chain). */
static void
loadPtrX (const asmop *aop, unsigned long offset)
{
  if (aop->type == AOP_LIT)
    {
      unsigned long addr = ulFromVal (aop->u.lit) + offset;

      emit ("ldi", "xh, #0x%02x", (unsigned) ((addr >> 8) & 0xFF));
      emit ("ldi", "xl, #0x%02x", (unsigned) (addr & 0xFF));
      return;
    }

  loadA (aop, 1, aop->size);  /* low byte */
  if (offset)
    {
      emit ("rec", "");
      emit ("adi", "a, #0x%02lx", offset);
    }
  emit ("sta", "xl");

  loadA (aop, 0, aop->size);  /* high byte */
  if (offset)
    emit ("adi", "a, #0x00");  /* propagate the low byte's carry-out */
  emit ("sta", "xh");
}

/*-----------------------------------------------------------------*/
/* genLH5801AssemblerStart - emit this port's one-off static        */
/* scratch/frame-pointer locations, unconditionally, up front        */
/*                                                                    */
/* Both __lh5801_cmp_scratch (genCmp()) and __lh5801_frame_ptr        */
/* (loadStackAddrToX() below) used to be emitted lazily, on first     */
/* use, via a one-shot ".area DATA"/label/".area CODE" directive       */
/* switch inline in gen.c's normal iCode-driven output stream. That    */
/* is exactly the same mistake genCmp()/genNot()'s own old fixed-name   */
/* helper labels were (see [[lh5801-sdcc-backend-phase1-gotchas]]):      */
/* whichever function happened to trigger the first use landed a real    */
/* (letter-starting) label definition mid-body, silently orphaning any    */
/* numbered SDCC temp label defined earlier in that same function from     */
/* every later branch referencing it -- confirmed directly compiling       */
/* device/lib/_muluchar.c (its "for" loop's genCmp() call triggered         */
/* ensureCmpScratch() mid-function, breaking a "jmp 00105$" a few             */
/* instructions later). Fixed by moving both declarations here -- called      */
/* from main.c's genAssemblerStart hook, which SDCCglue.c's glue() runs        */
/* once, unconditionally, before any function's code is emitted at all --       */
/* so they can never again land inside a function body. The extra 3 bytes        */
/* this costs on every compile (whether or not either is ever actually            */
/* used) is a trivial, deliberate trade for guaranteed correctness, matching      */
/* every other phase-1 design choice in this file.                                 */
/*-----------------------------------------------------------------*/
void
genLH5801AssemblerStart (FILE *of)
{
  fprintf (of, "\t.area DATA\n");
  fprintf (of, "__lh5801_cmp_scratch:\n");
  fprintf (of, "\t.ds 1\n");
  fprintf (of, "__lh5801_frame_ptr:\n");
  fprintf (of, "\t.ds 2\n");
  /* genShift()'s runtime (non-constant) shift-count loop counter --
     same per-module intra-function-only scratch category as
     __lh5801_cmp_scratch above (written and consumed entirely within
     one function's own generated code, never carried across a call
     boundary), so a plain per-module byte is correct here, unlike
     __lh5801_ret2.._ret7's genuinely cross-module storage in
     device/lib/lh5801/_lh5801_ret.asm. */
  fprintf (of, "__lh5801_shift_scratch:\n");
  fprintf (of, "\t.ds 1\n");
  /* opA()'s AND/ORA/EOR-with-a-register scratch, and genCmp()'s signed-
     comparison byte -- same per-module intra-iCode-only scratch category
     as the bytes above. (The third byte is spare: it was the sign
     bookkeeping genCmp() no longer needs. Kept, so every module's DATA
     layout stays what it was.) */
  fprintf (of, "__lh5801_op_scratch:\n");
  fprintf (of, "\t.ds 1\n");
  fprintf (of, "__lh5801_cmp_signA:\n");
  fprintf (of, "\t.ds 1\n");
  fprintf (of, "__lh5801_spare:\n");
  fprintf (of, "\t.ds 1\n");
  /* aopToDir()'s two scratch pointers -- same intra-iCode-only category. */
  fprintf (of, "__lh5801_remat0:\n");
  fprintf (of, "\t.ds 2\n");
  fprintf (of, "__lh5801_remat1:\n");
  fprintf (of, "\t.ds 2\n");
  fprintf (of, "\t.area CODE\n");

  /* Return-value bytes beyond the first two (which keep using A/X) pass
     through __lh5801_ret2.._ret7 -- see genReturn()'s/genCall()'s own
     comments for why this, and not a hidden-pointer parameter, is the
     right fix, and device/lib/lh5801/_lh5801_ret.asm's comment for why
     their *storage* is declared there (a real, dedicated, always-linked
     module -- matching hc08/s08's own device/lib/{hc08,s08}/_ret.c
     precedent for the identical problem) rather than emitted here: a
     plain per-module .area DATA declaration here, like
     __lh5801_cmp_scratch/__lh5801_frame_ptr above, would give every
     compiled module its own private, unshared byte -- fine for those
     two (always written then immediately read within one function's
     own generated code) but wrong here, where the whole point is
     carrying a value from a callee's module to a different caller's
     module. Confirmed both ways: a plain .area DATA declaration here
     silently returned garbage for every byte beyond the first two
     across a real cross-module call, and .area OSEG (this toolchain's
     usual same-address-overlay mechanism) doesn't help either --
     sdld's "Multiple definition" check applies before OSEG's placement
     logic ever gets a say (see sdas/linksrc/lksym.c's newsym()). */
}

/*-----------------------------------------------------------------*/
/* loadStackAddrToX - compute the runtime address of one byte of a  */
/* stack-resident symbol into X                                      */
/*                                                                    */
/* "stackOffset" is sym->stack (SDCCmem.c's allocParms()/the          */
/* __va_start placeholder -- both computed the same way for this      */
/* port's chosen stack.direction=-1/call_overhead=2) plus 1 (this      */
/* port's stack.offset -- S always points one *past* the last byte     */
/* actually pushed, confirmed against pc1500emu's push8()) plus        */
/* whichever byte index within that symbol is wanted, using the SAME    */
/* big-endian "0 = first/most-significant byte" convention dirAddr()    */
/* uses -- see genIpush()'s own comment for why pushing LSB-first        */
/* makes that line up exactly. Same REC+ADI carry-chain idiom            */
/* genGetValueAtAddress() already uses to add a compile-time-constant     */
/* offset to a base address.                                              */
/*-----------------------------------------------------------------*/
static void
loadStackAddrToX (long stackOffset)
{
  wassertl (stackOffset >= 0 && stackOffset <= 0xFF,
    "lh5801: phase-1 backend only supports an 8-bit stack-frame offset");

  emit ("lda", "(__lh5801_frame_ptr + 1)");  /* low byte */
  emit ("rec", "");
  emit ("adi", "a, #0x%02lx", (unsigned long) stackOffset);
  emit ("sta", "xl");

  emit ("lda", "(__lh5801_frame_ptr)");       /* high byte */
  emit ("adi", "a, #0x00");                    /* propagate carry-out */
  emit ("sta", "xh");
}

/*-----------------------------------------------------------------*/
/* genFunction - generate the prologue                             */
/*                                                                   */
/* A reentrant or variadic function's own named parameters are on   */
/* the real hardware stack (SDCCmem.c's allocParms() -- HASVARARGS   */
/* forces ISREENT, confirmed in SDCCsymt.c's processFunc()), not at  */
/* a fixed static address like every other phase-1 operand. ralloc.c */
/* already gave each such parameter its own static shadow location   */
/* (createStaticSpil(), the exact same "spillLoc" redirect mechanism  */
/* aopOp() already checks for spilt iTemps) -- this prologue's job    */
/* is just to copy the real, caller-pushed value into that shadow      */
/* once, before anything else in the function runs, so every later     */
/* iCode referencing the parameter transparently reads a normal,        */
/* fixed-address operand exactly as if it had never been on the stack   */
/* at all.                                                                */
/*-----------------------------------------------------------------*/
static void
genFunction (const iCode *ic)
{
  symbol *sym = OP_SYMBOL (IC_LEFT (ic));
  value *arg;
  bool anyOnStack = false;

  emit (";", "-----------------------------------------");
  emit (";", " function %s", sym->name);
  emit (";", "-----------------------------------------");

  emit ("", "%s:", sym->rname);
  genLine.lineCurr->isLabel = 1;

  for (arg = FUNC_ARGS (sym->type); arg; arg = arg->next)
    if (arg->sym && arg->sym->onStack)
      {
        anyOnStack = true;
        break;
      }

  if (!anyOnStack)
    return;

  emit ("ldx", "s");
  emit ("lda", "xh");
  emit ("sta", "(__lh5801_frame_ptr)");
  emit ("lda", "xl");
  emit ("sta", "(__lh5801_frame_ptr + 1)");

  for (arg = FUNC_ARGS (sym->type); arg; arg = arg->next)
    {
      symbol *psym = arg->sym;
      symbol *shadow;
      int size, j;

      if (!psym || !psym->onStack)
        continue;

      wassertl (psym->isspilt && psym->usl.spillLoc,
        "lh5801: on-stack parameter has no shadow static location "
        "(ralloc.c should have already created one)");
      shadow = psym->usl.spillLoc;
      size = getSize (psym->type);

      for (j = 0; j < size; j++)
        {
          loadStackAddrToX (psym->stack + 1 + j);
          emit ("lin", "x");
          emit ("sta", "(%s + %d)", shadow->rname, j);
        }
    }
}

/*-----------------------------------------------------------------*/
/* genEndFunction - generate the epilogue                          */
/*-----------------------------------------------------------------*/
static void
genEndFunction (const iCode *ic)
{
  (void) ic;
  emit ("rtn", "");
}

/*-----------------------------------------------------------------*/
/* genReturn - generate code for a return statement                */
/*                                                                   */
/* Return-value convention (phase 1, no register allocation yet):  */
/* 0 bytes = nothing to do; 1 byte = A; 2 bytes = X, with XH holding */
/* the most-significant byte and XL the least-significant, matching */
/* the big-endian memory layout genAssign() already uses everywhere */
/* (see litByte()'s comment). Whatever eventually generates CALL/   */
/* PCALL codegen must read a called function's result from the same */
/* place.                                                            */
/*-----------------------------------------------------------------*/
static void
genReturn (const iCode *ic)
{
  operand *left = IC_LEFT (ic);

  if (left)
    {
      aopOp (left);
      aopToDir (left, 0);

      wassertl (left->aop->size <= 8,
        "lh5801: phase-1 backend only supports 0- to 8-byte return values");

      if (left->aop->size == 1)
        loadA (left->aop, 0, 1);
      else if (left->aop->size >= 2)
        {
          /* Bytes 0-1 always go through X, exactly as before (size==2's
             sole case, historically) -- bytes 2+ (only possible when
             size > 2) go through the fixed __lh5801_retN pass-through
             locations declared once in genLH5801AssemblerStart(). */
          if (left->aop->type == AOP_LIT)
            {
              emit ("ldi", "xh, 0x%02x", litByte (left->aop, 0, left->aop->size));
              emit ("ldi", "xl, 0x%02x", litByte (left->aop, 1, left->aop->size));
            }
          else
            {
              loadA (left->aop, 0, left->aop->size);
              emit ("sta", "xh");
              loadA (left->aop, 1, left->aop->size);
              emit ("sta", "xl");
            }

          if (left->aop->size > 2)
            {
              int i;

              for (i = 2; i < left->aop->size; i++)
                {
                  loadA (left->aop, i, left->aop->size);
                  emit ("sta", "(__lh5801_ret%d)", i);
                }
            }
        }

      freeAsmop (left);
    }

  /* JMP (absolute, unlimited range), not BCH (8-bit relative branch) --
     confirmed necessary compiling a real, longer function
     (device/lib/_ulong2fs.c): an early "return" well before the
     function's end can be arbitrarily far from the shared epilogue this
     jumps to, and BCH silently has the exact same range limit genIfx()
     already has to route around for conditional branches
     (emitLongCondBranch()) -- unlike that case, this jump is already
     unconditional, so there's no inverted-condition trick needed, just
     a plain absolute jump. */
  if (!(ic->next && ic->next->op == LABEL && IC_LABEL (ic->next) == returnLabel))
    emit ("jmp", "!tlabel", labelKey2num (returnLabel->key));
}

/*-----------------------------------------------------------------*/
/* genLabel - generate a label                                     */
/*-----------------------------------------------------------------*/
static void
genLabel (const iCode *ic)
{
  if (IC_LABEL (ic) == entryLabel)
    return;

  emitLbl (IC_LABEL (ic));
}

/*-----------------------------------------------------------------*/
/* genGoto - generate an unconditional jump                        */
/*-----------------------------------------------------------------*/
static void
genGoto (const iCode *ic)
{
  /* jmp (S_TYPABS, absolute 16-bit target), not bch (S_TYPBRA, an 8-bit
     signed relative displacement) -- a goto's target can be arbitrarily
     far away in a large function (a loop back-edge, a distant switch
     case, ...), and bch's limited range is exactly what produced
     aslh5801's "Branching Range Exceeded" errors compiling
     device/lib/printf_large.c. jmp has no such limit. */
  emit ("jmp", "!tlabel", labelKey2num (IC_LABEL (ic)->key));
}

/*-----------------------------------------------------------------*/
/* genIpush - push one call argument onto the real hardware stack  */
/*-----------------------------------------------------------------*/
static void
genIpush (const iCode *ic)
{
  operand *left = IC_LEFT (ic);
  int size, i;

  wassertl (ic->parmPush,
    "lh5801: only argument-passing IPUSH is supported in the phase-1 backend");

  aopOp (left);
  size = left->aop->size;

  /* Pushed LSB-first, so that once genFunction()'s prologue snapshots S
     into a frame pointer, the pushed bytes read back out in the SAME
     big-endian order (offset 0 = MSB) as every other memory operand in
     this backend: push8() (pc1500emu's CPU core) writes at the current S
     and *then* decrements, so whichever byte is pushed *last* ends up at
     the *lowest* address. Pushing LSB-first here means the MSB is pushed
     last, landing at the lowest address -- i.e. offset 0 from the frame
     pointer, matching dirAddr()'s own convention exactly (see
     loadStackAddrToX()). genCall()'s cleanup below just discards
     ic->parmBytes bytes without caring about order, so getting this
     backwards would have been invisible until something actually read a
     stack-passed parameter's value -- which nothing did until va_start/
     va_arg and reentrant-parameter support were added. */
  for (i = size - 1; i >= 0; i--)
    {
      loadA (left->aop, i, size);
      emit ("psh", "a");
    }

  freeAsmop (left);
}

/*-----------------------------------------------------------------*/
/* genCall - generate a direct (CALL) or indirect (PCALL) call     */
/*                                                                   */
/* Arguments were already pushed onto the real hardware stack by   */
/* preceding IPUSH iCodes (see genIpush above); ic->parmBytes says  */
/* how many bytes to discard afterward -- caller cleans up, again   */
/* just an internal convention invented for phase 1.                */
/*                                                                   */
/* PCALL (calling through a function-pointer *variable* -- needed   */
/* for e.g. printf's internal pfn_outputchar callback in             */
/* device/lib/printf_large.c): the LH5801 has no register-indirect  */
/* call or jump instruction at all (confirmed against the sdas      */
/* mnemonic table -- JMP/SJP are absolute-16-bit-target only), so    */
/* this uses the standard trampoline trick for CPUs without one: a  */
/* single shared "jmp 0x0000" whose 2-byte address operand gets     */
/* patched with the real target immediately before each indirect    */
/* call, reached via a real SJP. Using SJP (not another JMP) to      */
/* reach the trampoline is essential: SJP pushes *our* real return   */
/* address, and the trampoline's own JMP doesn't touch the stack at  */
/* all, so the callee's eventual RTN pops the right address --       */
/* using SJP for the trampoline's inner jump too would instead push  */
/* a second, useless return address and break the stack.             */
/*-----------------------------------------------------------------*/

/*-----------------------------------------------------------------*/
/* genAddrOf - generate code for '&' (address-of)                  */
/*                                                                   */
/* Needed for e.g. `printf("literal string")`: the string literal   */
/* lives in CODE space under its own compiler-generated symbol, and */
/* taking its address is exactly this iCode. Phase 1 has no real    */
/* register allocation, so the computed address is just written out */
/* to the result's memory location a byte at a time, the same way   */
/* every other gen*() here does -- there's no need to actually route */
/* it through a register first. The assembler's shared expr()       */
/* parser (asxxsrc/asexpr.c, confirmed via a standalone test) already */
/* supports the standard sdas ">expr"/"<expr" high/low-byte-of-a-    */
/* link-time-address operators, including with a symbol+offset       */
/* expression, so this needs no new assembler support at all.        */
/*-----------------------------------------------------------------*/
static void
genAddrOf (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  symbol *sym;
  long offset = 0;
  char sym_expr[800]; /* rname can be up to SDCC_NAME_MAX (SDCCsymt.h) long */

  wassertl (IS_SYMOP (left), "lh5801: ADDRESS_OF operand must be a symbol");
  sym = OP_SYMBOL (left);

  if (IC_RIGHT (ic))
    {
      wassertl (IS_OP_LITERAL (IC_RIGHT (ic)),
        "lh5801: ADDRESS_OF with a non-constant offset is not yet supported in the phase-1 backend");
      offset = (long) ulFromVal (OP_VALUE (IC_RIGHT (ic)));
    }

  /* __va_start (SDCCmem.c's ISO C23 va_start placeholder, see
     lh5801_sdcc_backend_phase1_gotchas memory) is on-stack too, but
     unlike a named parameter it has no fixed size to shadow-copy at
     function entry (see genFunction()) -- its whole value *is* its
     address (an unspecified-length array, decaying to a pointer exactly
     the way any other array does for `arg = __va_start;`), computed
     fresh, at runtime, every time it's taken. Same "frame pointer +
     compile-time-constant offset" as genFunction()'s shadow-copy, just
     stored directly into the result instead of dereferenced further.

     A named parameter, though, has that static shadow copy, and every
     other use of it reads and writes the shadow -- so its address must
     be the shadow's too, or a write through the pointer goes to the stack
     copy nobody reads again. (sprintf()'s `&buf` was exactly this: the
     output pointer advanced on the stack while `*buf = 0` used the
     shadow, still at the start, and overwrote the first character.) */
  if (sym->onStack && sym->isspilt && sym->usl.spillLoc)
    sym = sym->usl.spillLoc;
  else if (sym->onStack)
    {
      wassertl (offset >= 0,
        "lh5801: ADDRESS_OF a stack symbol needs a non-negative offset");

      loadStackAddrToX (sym->stack + 1 + offset);

      aopOp (result);
      wassertl (result->aop->size == 2, "lh5801: ADDRESS_OF result must be a 2-byte pointer");

      emit ("lda", "xh");
      storeA (result->aop, 0);

      emit ("lda", "xl");
      storeA (result->aop, 1);

      freeAsmop (result);
      return;
    }

  if (offset)
    snprintf (sym_expr, sizeof (sym_expr), "(%s + %ld)", sym->rname, offset);
  else
    snprintf (sym_expr, sizeof (sym_expr), "%s", sym->rname);

  aopOp (result);
  wassertl (result->aop->size == 2, "lh5801: ADDRESS_OF result must be a 2-byte pointer");

  emit ("ldi", "a, >%s", sym_expr);
  storeA (result->aop, 0);

  emit ("ldi", "a, <%s", sym_expr);
  storeA (result->aop, 1);

  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genCast - generate code for an explicit or implicit type cast   */
/*                                                                   */
/* Same-size casts (e.g. pointer-to-pointer, needed for printf's     */
/* internal `(const char generic*)` conversions of string-literal/   */
/* code-space addresses -- every pointer is represented the same     */
/* flat 2-byte way regardless of what address space it nominally     */
/* points into, since this backend has no real banking/segment       */
/* support) are a plain byte-for-byte copy, identical to genAssign(). */
/*                                                                   */
/* Narrowing (result smaller than source) keeps the low-order        */
/* result-size bytes and discards the rest, matching plain C integer */
/* truncation -- in our big-endian layout (byte 0 = MSB), that's the  */
/* *last* result-size bytes of the source.                           */
/*                                                                   */
/* Widening (result bigger than source): see genWiden().             */
/*-----------------------------------------------------------------*/

/* result = the constant `lit`, byte i being the constant's own byte i at
   the result's width (so narrowing and widening both come out right):
   into a register half with LDI directly, not through A; into memory
   through A, loaded only when a byte differs from the one before -- and a
   lone 0 or FFH byte with ANI/ORI (ab),i instead (4 bytes, not 5). */
static void
storeLiteral (const asmop *result, const asmop *lit)
{
  int size = result->size;
  int inA = -1;
  int i;

  for (i = 0; i < size; i++)
    {
      unsigned char v = litByte (lit, i, size);
      int r = aopReg (result, i);

      if (r > LH5801_REG_A)
        {
          emit ("ldi", "%s, 0x%02x", regName[r], v);
          continue;
        }
      if (r < 0 && result->type == AOP_DIR && size == 1 && (v == 0x00 || v == 0xFF))
        {
          char buf[128];

          dirAddr (buf, sizeof (buf), result, i);
          emit (v ? "ori" : "ani", "(%s), 0x%02x", buf, v);
          continue;
        }
      if (v != inA)
        {
          emit ("ldi", "a, 0x%02x", v);
          inA = v;
        }
      storeA (result, i);
    }
}

/* Stores `right` into the wider `result`: an unsigned source zero-
   extends, a signed one sign-extends (its sign bit copied into every
   new high-order byte), and a literal is simply written at the result's
   full width -- byteOfVal() already knows its value's real high bytes.
   Zero-extending everything (as this used to) silently turned
   `long v = -7;`-style widening of a signed variable into 65529. */
static void
genWiden (operand *result, operand *right)
{
  int resultSize = result->aop->size;
  int rightSize = right->aop->size;
  int extra = resultSize - rightSize;
  int i;

  if (right->aop->type == AOP_LIT)
    {
      storeLiteral (result->aop, right->aop);
      return;
    }

  if ((right->aop->type == AOP_DIR || right->aop->type == AOP_REG) &&
      !SPEC_USIGN (getSpec (operandType (right))) && !IS_PTR (operandType (right)))
    {
      symbol *positive = newiTempLabel (NULL);

      loadA (right->aop, 0, rightSize);
      emit ("ani", "a, #0x80");
      emit ("bzs", "!tlabel", labelKey2num (positive->key));
      emit ("ldi", "a, 0xff");
      emitLbl (positive);
    }
  else
    emit ("ldi", "a, 0x00");

  for (i = 0; i < extra; i++)
    {
      storeA (result->aop, i);
    }

  for (i = 0; i < rightSize; i++)
    {
      loadA (right->aop, i, rightSize);
      storeA (result->aop, extra + i);
    }
}

static void
genCast (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  int resultSize, rightSize, i;

  aopOp (right);
  aopOp (result);

  resultSize = result->aop->size;
  rightSize = right->aop->size;

  if (right->aop->type == AOP_LIT)
    storeLiteral (result->aop, right->aop);
  else if (resultSize <= rightSize)
    {
      int srcOffset = rightSize - resultSize;

      for (i = 0; i < resultSize; i++)
        {
          loadA (right->aop, srcOffset + i, rightSize);
          storeA (result->aop, i);
        }
    }
  else
    genWiden (result, right);

  freeAsmop (right);
  freeAsmop (result);
}

static void
genCall (const iCode *ic)
{
  static bool pcallTrampolineEmitted = false;
  int parmBytes = ic->parmBytes;

  if (ic->op == PCALL)
    {
      operand *left = IC_LEFT (ic);

      /* (Not in a dry run: that would mark it emitted without emitting it.) */
      if (!pcallTrampolineEmitted && !regalloc_dry_run)
        {
          /* A plain fixed name, not SDCC's usual newiTempLabel()/!tlabel
             numbered-local-label mechanism -- that combination tripped
             aslh5801's branch-target-area check here for reasons not
             fully root-caused (genReturn/genGoto's own forward "bch
             !tlabel" branches elsewhere work fine, so it's specific to
             this call site somehow). This block is only ever emitted
             once for the whole file (guarded by pcallTrampolineEmitted),
             so a fixed name can't collide with anything. */
          pcallTrampolineEmitted = true;
          emit ("bch", "__lh5801_pcall_trampoline_skip");
          emit ("", "__lh5801_pcall_trampoline:");
          genLine.lineCurr->isLabel = 1;
          emit ("jmp", "0x0000");
          emit ("", "__lh5801_pcall_trampoline_skip:");
          genLine.lineCurr->isLabel = 1;
        }

      aopOp (left);
      aopToDir (left, 0);
      wassertl (left->aop->size == 2, "lh5801: PCALL target must be a 2-byte function pointer");

      loadA (left->aop, 0, left->aop->size);
      emit ("sta", "(__lh5801_pcall_trampoline + 1)");

      loadA (left->aop, 1, left->aop->size);
      emit ("sta", "(__lh5801_pcall_trampoline + 2)");

      freeAsmop (left);

      emit ("sjp", "__lh5801_pcall_trampoline");
    }
  else
    {
      symbol *sym = OP_SYMBOL (IC_LEFT (ic));

      emit ("sjp", "%s", sym->rname[0] ? sym->rname : sym->name);
    }

  if (IC_RESULT (ic))
    {
      operand *result = IC_RESULT (ic);
      sym_link *funcType;
      int resultSize, calleeSize, skip, i;

      aopOp (result);
      resultSize = result->aop->size;

      /* The callee always returns according to ITS OWN declared type's
         width (genReturn()'s convention: 1 byte in A, 2 in X, more in
         __lh5801_retN), regardless of how many bytes IC_RESULT itself
         keeps -- e.g. `narrowField += wideFunc();`-style truncation
         narrows IC_RESULT's own operand size without changing what the
         callee actually does. Reading based on IC_RESULT's (possibly
         narrower) size instead of the real callee return width misreads
         a genuinely-2-byte-or-wider return as if it were 1 byte,
         silently picking up whatever garbage is left in A instead of
         the real value in X -- confirmed the hard way bringing up
         device/lib/_mullong.c's `t.b.b3 += bcast(a)->b.b3 *
         bcast(b)->b.b0;` (an unsigned-int-returning __muluchar() call
         truncated into a 1-byte struct field). Fixed by always reading
         the full callee-width sequence, keeping only its trailing
         (least-significant, per this port's big-endian byte order)
         resultSize bytes -- matching C's own truncation-keeps-low-
         -order-bits semantics. */
      if (ic->op == PCALL)
        funcType = operandType (IC_LEFT (ic))->next->next;
      else
        funcType = OP_SYMBOL (IC_LEFT (ic))->type->next;
      calleeSize = IS_VOID (funcType) ? 0 : getSize (funcType);

      wassertl (calleeSize <= 8,
        "lh5801: phase-1 backend only supports 0- to 8-byte call results");
      wassertl (resultSize <= calleeSize,
        "lh5801: call result operand wider than the callee's own return type");

      skip = calleeSize - resultSize;

      for (i = 0; i < calleeSize; i++)
        {
          if (i == 0 && calleeSize > 1)
            emit ("lda", "xh");
          else if (i == 1)
            emit ("lda", "xl");
          else if (i >= 2)
            emit ("lda", "(__lh5801_ret%d)", i);
          /* i == 0 && calleeSize == 1: already sitting in A, no load needed. */

          if (i >= skip)
            {
              storeA (result->aop, i - skip);
            }
        }

      freeAsmop (result);
    }

  /* The pushed arguments come off only after the result is stored: POP A
     and POP X would otherwise overwrite the value the callee returned in
     A/X -- every call with stack arguments (reentrant or varargs) that
     used its result got the last argument byte instead. */
  while (parmBytes >= 2)
    {
      emit ("pop", "x");
      parmBytes -= 2;
    }
  if (parmBytes)
    emit ("pop", "a");
}

/*-----------------------------------------------------------------*/
/* genAssign - generate code for '='                                */
/*-----------------------------------------------------------------*/
static void
genAssign (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  int resultSize, rightSize, i;

  aopOp (right);
  aopOp (result);

  resultSize = result->aop->size;
  rightSize = right->aop->size;

  /* A plain '=' can itself be narrowing or widening, not just same-size
     -- e.g. `char sign = SIGN(a) ^ SIGN(b);` narrowing a 4-byte
     (unsigned long) XOR result into a 1-byte destination (confirmed via
     device/lib/_fsmul.c's own such line). Unlike an explicit CAST
     (genCast() above, which already gets this right), plain assignment
     used to assume same-size always and just copy destSize bytes
     starting from right's OWN byte 0 -- silently keeping the *leading*
     (most-significant) bytes of a wider source instead of C's actual
     truncation semantics (keep the *trailing*, least-significant bytes,
     per this port's big-endian byte order). Mirrors genCast()'s exact
     algorithm since the needed behavior is identical either way. */
  if (right->aop->type == AOP_LIT)
    storeLiteral (result->aop, right->aop);
  else if (resultSize <= rightSize)
    {
      int srcOffset = rightSize - resultSize;

      for (i = 0; i < resultSize; i++)
        {
          loadA (right->aop, srcOffset + i, rightSize);
          storeA (result->aop, i);
        }
    }
  else
    genWiden (result, right);

  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genPointerSet - generate code for a POINTER_SET '=' iCode, i.e.  */
/* "*ptr = value" / "arr[k] = value" once the address has already   */
/* been computed into IC_RESULT -- the write-side mirror of         */
/* genGetValueAtAddress() below.                                    */
/*                                                                   */
/* Without this, genLH5801Code()'s '=' case fell through to plain   */
/* genAssign() for POINTER_SET iCodes too, which treats IC_RESULT's  */
/* own aop as the direct destination -- i.e. it clobbered the        */
/* computed POINTER variable itself with the value instead of        */
/* storing through it, silently leaving the real target (e.g. every  */
/* constant-index array-element write, such as printf_large.c's      */
/* "value.byte[4] = 0;") untouched. Confirmed via a minimal           */
/* "arr[4] = 0xAA;" repro: the old codegen emitted two "sta" straight */
/* into the address temp and never touched X or _arr at all.          */
/*                                                                    */
/* IC_RESULT (the address being stored through) can itself be a       */
/* compile-time-constant AOP_LIT, not just a real AOP_DIR variable      */
/* holding a pointer value -- confirmed via a minimal                   */
/* "*(unsigned char *)0x4700 = 0x55;" repro (a fixed hardware/memory     */
/* address, an extremely common embedded idiom -- and the same shape      */
/* any "*(ptr + N) = v" where N and ptr are both compile-time-known         */
/* constant-folds down to). dirAddr() assumes a real AOP_DIR symbol and      */
/* asserts otherwise, so that case needs its own literal-immediate X load.   */
/*-----------------------------------------------------------------*/
static void
genPointerSet (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  char pair;

  aopOp (right);
  aopOp (result);

  wassertl (result->aop->size == 2, "lh5801: pointer dereference requires a 2-byte pointer");

  size = right->aop->size;

  if (result->aop->type == AOP_IMMD)
    {
      /* A constant address: store to it directly, no X at all. */
      for (i = 0; i < size; i++)
        {
          loadA (right->aop, i, size);
          emit ("sta", "(%s + %d)", result->aop->u.dir, i);
        }
      freeAsmop (right);
      freeAsmop (result);
      return;
    }

  pair = regPair (result->aop);
  if (pair && !aopUsesPair (right->aop, pair))
    {
      /* The pointer is already in X, Y or U: store through it there. */
      for (i = 0; i < size; i++)
        {
          loadA (right->aop, i, size);
          emit ("sin", "%c", pair);
        }
      if (aopSurvives (ic, result->aop))
        for (i = 0; i < size; i++)
          emit ("dec", "%c", pair);
      freeAsmop (right);
      freeAsmop (result);
      return;
    }

  loadPtrX (result->aop, 0);

  for (i = 0; i < size; i++)
    {
      loadA (right->aop, i, size);
      emit ("sin", "x");
    }

  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genAddSub - generate code for '+' and '-'                       */
/*                                                                   */
/* Byte-by-byte, LSB first (byte size-1, matching this port's       */
/* big-endian layout), chained via the carry flag -- REC/SEC first  */
/* to start the chain with carry-in 0 (add) / 1 (subtract, per the  */
/* CPU's SBC = A + ~operand + carry two's-complement identity, see  */
/* pc1500emu's doAdd()), then ADC/SBC per byte. A literal right      */
/* operand uses ADI/SBI's direct "A,i" immediate form instead --     */
/* confirmed via the sdas mnemonic table that ADC/SBC themselves     */
/* have no immediate form at all (memory-operand only).              */
/*-----------------------------------------------------------------*/
static bool genAddSubInPlace (const asmop *result, const asmop *left, const asmop *right, bool isSub);

static void
genAddSub (const iCode *ic, bool isSub)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  int leftExtra, rightExtra;

  aopOp (left);
  aopOp (right);
  aopOp (result);
  aopToDir (right, 0);

  if (genAddSubInPlace (result->aop, left->aop, right->aop, isSub))  /* x += k in place */
    {
      freeAsmop (left);
      freeAsmop (right);
      freeAsmop (result);
      return;
    }

  size = result->aop->size;
  /* AOP_LIT operands are exempt: litByte()/byteOfVal() extract whatever
     byte is asked for straight from the literal's real numeric value
     (confirmed via `charsOutputted++` in device/lib/printf_large.c,
     where charsOutputted is a 2-byte int but the literal 1 gets sized as
     a 1-byte AOP_LIT -- the *value* 1 is still perfectly well-defined at
     byte 0 for a 2-byte operation, nothing to reject here).

     A mismatched AOP_DIR/AOP_FUNC operand can only be *narrower* than
     the result, never wider (SDCC's own usual-arithmetic-conversion
     rules never shrink an operand for a '+'/'-'), and gets zero-extended
     -- needed for e.g. `ptr + narrowIndex` (a byte-sized array index
     added to a 2-byte pointer, confirmed needed compiling any
     `arr[i]`-style indexing where `i` is narrower than the pointer --
     SDCC represents this as a plain '+' iCode with mismatched operand
     sizes, expecting the backend to widen, not an explicit CAST first).
     Signed widening isn't handled (matches genCast()'s own documented
     limitation) -- every real caller so far is unsigned. */
  wassertl (left->aop->type == AOP_LIT || left->aop->size <= size,
    "lh5801: phase-1 backend can only widen (never narrow) an operand in +/-");
  wassertl (right->aop->type == AOP_LIT || right->aop->size <= size,
    "lh5801: phase-1 backend can only widen (never narrow) an operand in +/-");
  leftExtra = (left->aop->type == AOP_LIT) ? 0 : size - left->aop->size;
  rightExtra = (right->aop->type == AOP_LIT) ? 0 : size - right->aop->size;

  emit (isSub ? "sec" : "rec", "");

  for (i = size - 1; i >= 0; i--)
    {
      if (i < leftExtra)
        emit ("ldi", "a, 0x00");
      else
        loadA (left->aop, i - leftExtra, left->aop->size);

      if (right->aop->type == AOP_LIT)
        opA (isSub ? "sbc" : "adc", isSub ? "sbi" : "adi", right->aop, i, size);
      else if (i < rightExtra)
        /* No real memory address holds this zero-extension byte -- add/
           subtract a literal 0 instead, still propagating the carry
           chain exactly like a real byte would. */
        emit (isSub ? "sbi" : "adi", "a, #0x00");
      else
        opA (isSub ? "sbc" : "adc", isSub ? "sbi" : "adi", right->aop, i - rightExtra, right->aop->size);

      storeA (result->aop, i);
    }

  freeAsmop (left);
  freeAsmop (right);
  freeAsmop (result);
}

/* The IFX that a `x & k` bit test (genBitwise()) generates itself, or
   NULL: an AND of a variable with a constant that has exactly one nonzero
   byte, whose result nothing but the IFX after it uses (ifxForOp()). Also
   ralloc2.cc's extra_ic_generated() -- the two must agree, or the IFX is
   generated twice or not at all. */
iCode *
lh5801_bitTest (const iCode *ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i, bytes = 0;

  if ((ic->op != '&' && ic->op != BITWISEAND) || !IS_OP_LITERAL (right) || !IS_SYMOP (left) ||
      isRemat (left) || !IS_SYMOP (IC_RESULT (ic)))
    return NULL;
  size = getSize (operandType (left));
  if (size < 1 || size > 4)
    return NULL;
  for (i = 0; i < size; i++)
    if (byteOfVal (OP_VALUE (right), i))
      bytes++;
  if (bytes != 1)
    return NULL;
  return ifxForOp (IC_RESULT (ic), ic);
}

/* Do two operands name the very same storage? */
static bool
sameStorage (const asmop *a, const asmop *b)
{
  int i;

  if (a->type != b->type || a->size != b->size)
    return false;
  if (a->type == AOP_DIR)
    return !strcmp (a->u.dir, b->u.dir);
  if (a->type == AOP_REG)
    {
      for (i = 0; i < a->size; i++)
        if (a->u.regs[i] != b->u.regs[i])
          return false;
      return true;
    }
  return false;
}

/* `x = x + k` / `x = x - k` for a constant k, changing x where it is,
   without going through A: in memory ADI (ab),i, then -- for a wider
   value -- the carry passed up a byte at a time (k's upper bytes must be
   all 0, adding 1 on a carry, or all FFH, subtracting 1 when there was
   none: memory ADI takes no carry in, but sets it); a value in a register
   half INC/DEC (1 byte, 2 for an H half); a 2-byte value filling a pointer
   register INC/DEC of the pair (1 byte). False when none of those fits
   (or wouldn't be smaller); the caller then does the ordinary add. */
static bool
genAddSubInPlace (const asmop *result, const asmop *left, const asmop *right, bool isSub)
{
  int size = result->size;
  unsigned long k, mask;
  char pair;
  int i;

  if (right->type != AOP_LIT || !sameStorage (result, left) || size > 4)
    return false;

  mask = size == 4 ? 0xFFFFFFFFul : (1ul << (8 * size)) - 1;
  k = ulFromVal (right->u.lit);
  if (isSub)
    k = 0 - k;
  k &= mask;
  if (k == 0)
    return true;

  if (result->type == AOP_REG)
    {
      unsigned long neg = (0 - k) & mask;
      int n;

      pair = regPair (result);
      if (size == 1 || pair)
        {
          if (k <= (pair ? 3u : 2u))
            n = (int) k;
          else if (neg <= (pair ? 3u : 2u))
            n = -(int) neg;
          else
            return false;
          for (i = 0; i < (n < 0 ? -n : n); i++)
            {
              if (pair)
                emit (n < 0 ? "dec" : "inc", "%c", pair);
              else
                emit (n < 0 ? "dec" : "inc", "%s", regName[result->u.regs[0]]);
            }
          return true;
        }
      return false;
    }

  if (result->type == AOP_DIR)
    {
      unsigned long upper = k >> 8;
      unsigned long ones = mask >> 8;
      char buf[128];
      symbol *done;

      if (size > 1 && upper != 0 && upper != ones)
        return false;
      dirAddr (buf, sizeof (buf), result, size - 1);
      emit ("adi", "(%s), 0x%02x", buf, (unsigned) (k & 0xFF));
      if (size == 1)
        return true;
      done = newiTempLabel (NULL);
      for (i = size - 2; i >= 0; i--)
        {
          /* adding: a carry means one more up; subtracting (k's upper
             bytes FFH): no carry means a borrow, one less up */
          emit (upper ? "bcs" : "bcr", "!tlabel", labelKey2num (done->key));
          dirAddr (buf, sizeof (buf), result, i);
          emit ("adi", "(%s), 0x%02x", buf, upper ? 0xFF : 0x01);
        }
      emitLbl (done);
      return true;
    }
  return false;
}

/*-----------------------------------------------------------------*/
/* genCmp - generate code for '>' '<' LE_OP GE_OP EQ_OP NE_OP       */
/*                                                                   */
/* Method: subtract byte-by-byte LSB-first via the same chainable    */
/* SBC/SBI carry-chain genAddSub() uses (discarding the arithmetic   */
/* result -- only the flags matter), while separately OR-accumulating */
/* each byte's subtraction result into a shared scratch byte to also */
/* get a reliable "were *all* bytes equal" signal (the final Z flag  */
/* after a carry chain only reflects the *last* byte, not the whole  */
/* multi-byte result, so it can't answer equality by itself). AND/OR */
/* only affect Z, not C (confirmed against pc1500emu's opcode         */
/* implementations), so interleaving the two doesn't corrupt either. */
/* After the loop: C=1 means left>=right (unsigned, no borrow); the  */
/* scratch byte is 0 iff left==right (true regardless of signedness  */
/* -- equality doesn't care about sign interpretation).              */
/*                                                                   */
/* For an UNSIGNED comparison that's the whole story: the switch     */
/* below decides '<'/'>='/'>'/'<=' directly from C, and '=='/'!=' from */
/* the scratch byte's Z.                                              */
/*                                                                    */
/* A SIGNED comparison: the LH5801 has no signed-overflow flag         */
/* (confirmed against pc1500emu's Flags struct: only c/v/z/h, and v     */
/* isn't the signed-arithmetic overflow flag x86/68k programmers         */
/* usually mean by that letter here), so the usual "N xor V" trick       */
/* isn't available. Instead both most-significant bytes are compared     */
/* with their sign bits flipped: x ^ 80H maps -128..127 onto 0..255 in    */
/* the same order, so the unsigned C/Z result above is then exactly the   */
/* signed one. (This replaced a sign-bookkeeping sequence about 40 bytes  */
/* longer.)                                                               */
/*                                                                    */
/* Fused with a following IFX (ifx non-NULL -- ifxForOp() found that  */
/* the 0/1 result is used by nothing but that branch): instead of      */
/* materializing the result byte for genIfx() to reload, branch        */
/* straight from the flags/scratch bytes (emitCmpBranch()). The usual   */
/* "if (a == b)" then costs a compare and a branch, not a compare, a    */
/* 0/1 store, a reload and a branch -- it was ~30 bytes per test.       */
/*                                                                    */
/* EQ_OP/NE_OP always take the unsigned path: equality doesn't depend  */
/* on how the bits are read, and plain char is signed in SDCC, so every */
/* `c == 'A'` used to pay for the whole signed sequence above. A single- */
/* byte unsigned comparison skips the all-bytes-equal scratch byte too:  */
/* the one SBC/SBI already leaves Z exact.                              */
/*-----------------------------------------------------------------*/
static void emitLongCondBranch (const char *invMnemonic, const symbol *target);

/* Jumps to `target` when the comparison `op` (as genCmp() leaves the
   flags for it -- signed or not, see genCmp()) is `when`. */
static void
emitCmpBranch (int op, bool when, const symbol *target)
{
  symbol *skip = newiTempLabel (NULL);

  if (!when)
    switch (op)
      {
      case '<': op = GE_OP; break;
      case GE_OP: op = '<'; break;
      case '>': op = LE_OP; break;
      case LE_OP: op = '>'; break;
      case EQ_OP: op = NE_OP; break;
      case NE_OP: op = EQ_OP; break;
      }

  /* C=1 (no borrow) means left>=right, Z means left==right. */
  switch (op)
    {
    case '<':
      emitLongCondBranch ("bcs", target);
      return;
    case GE_OP:
      emitLongCondBranch ("bcr", target);
      return;
    case EQ_OP:
      emitLongCondBranch ("bzr", target);
      return;
    case NE_OP:
      emitLongCondBranch ("bzs", target);
      return;
    case '>':
      emit ("bcr", "!tlabel", labelKey2num (skip->key));
      emit ("bzs", "!tlabel", labelKey2num (skip->key));
      emit ("jmp", "!tlabel", labelKey2num (target->key));
      emitLbl (skip);
      return;
    case LE_OP:
      {
        symbol *take = newiTempLabel (NULL);

        emit ("bcr", "!tlabel", labelKey2num (take->key));
        emit ("bzr", "!tlabel", labelKey2num (skip->key));
        emitLbl (take);
        emit ("jmp", "!tlabel", labelKey2num (target->key));
        emitLbl (skip);
      }
      return;
    }
  wassertl (0, "lh5801: unrecognized comparison operator in emitCmpBranch");
}

static void
genCmp (const iCode *ic, iCode *ifx)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  bool needScratch;
  int size, i;
  bool isSigned;
  /* newiTempLabel()/!tlabel, not a plain fixed-name label: aslh5801's
     numbered "NNNNN$" labels (which is how every *other* SDCC-emitted
     label in this function is rendered) are "reusable local symbols"
     scoped between real (letter-starting) label definitions (see
     asmain.c's digit-led-line handling and asexpr.c's term(), which
     look the number up via the *single* symp->s_tsym chain belonging
     to whichever real symbol was most recently defined). A plain,
     letter-starting fixed-name label defined here would itself become
     a new real-symbol boundary, silently orphaning every numbered
     label SDCC defined earlier in this same function from every
     branch that references it afterward -- producing exactly the
     "undefined symbol"/"not same area" errors this caused in
     printf_large.c. Using the same numbered-label mechanism as
     everything else keeps this function's reusable-symbol scope
     intact throughout. */
  symbol *trueLabel = newiTempLabel (NULL);
  symbol *endLabel = newiTempLabel (NULL);

  aopOp (left);
  aopOp (right);
  aopOp (result);
  aopToDir (right, 0);

  /* Same AOP_LIT-size exemption as genAddSub() -- a literal's own aop
     size can be smaller than the real operation width (SDCC sizes a
     literal to the smallest type that fits its value, not to whatever
     it's being compared against), but litByte() extracts the correct
     byte regardless. Prefer a real (AOP_DIR) operand's size as the
     canonical width; only fall back to comparing two literals' sizes
     directly (a case the constant folder would normally have already
     eliminated) if both sides are literal. Signedness is read from
     whichever side isn't a literal, for the same reason (a literal's
     own sym_link doesn't reliably reflect the comparison's real type);
     if both sides are literal, the constant folder would normally have
     already eliminated the comparison, so the unsigned path is fine. */
  if (left->aop->type != AOP_LIT)
    {
      size = left->aop->size;
      isSigned = !SPEC_USIGN (getSpec (operandType (left)));
    }
  else if (right->aop->type != AOP_LIT)
    {
      size = right->aop->size;
      isSigned = !SPEC_USIGN (getSpec (operandType (right)));
    }
  else
    {
      size = (left->aop->size > right->aop->size) ? left->aop->size : right->aop->size;
      isSigned = false;
    }

  if (ic->op == EQ_OP || ic->op == NE_OP)
    isSigned = false;
  needScratch = size > 1;

  wassertl ((left->aop->type == AOP_LIT || left->aop->size == size) &&
            (right->aop->type == AOP_LIT || right->aop->size == size),
    "lh5801: phase-1 backend requires matching operand sizes for comparisons");
  wassertl (result->aop->size == 1, "lh5801: comparison result must be 1 byte");

  /* Signed: the most-significant bytes are compared with their sign bits
     flipped (EAI 80H), which maps signed order onto unsigned order --
     so every case below is then just the unsigned one. EAI and LDA only
     touch Z, never the carry the subtraction chain is threading through.
     The right operand's flipped MSB goes to __lh5801_cmp_signA first,
     since SBC has no "operand xor 80H" form. */
  if (isSigned && right->aop->type != AOP_LIT)
    {
      loadA (right->aop, 0, right->aop->size);
      emit ("eai", "a, #0x80");
      emit ("sta", "(__lh5801_cmp_signA)");
    }

  if (needScratch)
    {
      emit ("ldi", "a, 0x00");
      emit ("sta", "(__lh5801_cmp_scratch)");
    }

  emit ("sec", "");

  for (i = size - 1; i >= 0; i--)
    {
      bool flip = isSigned && i == 0;

      loadA (left->aop, i, size);
      if (flip)
        emit ("eai", "a, #0x80");

      if (right->aop->type == AOP_LIT)
        emit ("sbi", "a, #0x%02x", litByte (right->aop, i, size) ^ (flip ? 0x80 : 0));
      else if (flip)
        emit ("sbc", "(__lh5801_cmp_signA)");
      else
        opA ("sbc", "sbi", right->aop, i, size);

      if (needScratch)
        {
          emit ("ora", "(__lh5801_cmp_scratch)");
          emit ("sta", "(__lh5801_cmp_scratch)");
        }
    }

  freeAsmop (left);
  freeAsmop (right);

  if (ifx)
    {
      if (IC_TRUE (ifx))
        emitCmpBranch (ic->op, true, IC_TRUE (ifx));
      else
        emitCmpBranch (ic->op, false, IC_FALSE (ifx));
      ifx->generated = 1;
      freeAsmop (result);
      return;
    }

    {
      /* C=1 (no borrow) means left>=right; Z (from the last ora/sta above)
         means left==right. Each case below jumps to trueLabel when its
         condition holds, falls through to "store 0" otherwise. */
      switch (ic->op)
        {
        case '<':                          /* left <  right : C=0 */
          emit ("bcr", "!tlabel", labelKey2num (trueLabel->key));
          break;
        case GE_OP:                        /* left >= right : C=1 */
          emit ("bcs", "!tlabel", labelKey2num (trueLabel->key));
          break;
        case EQ_OP:                        /* left == right : Z=1 */
          emit ("bzs", "!tlabel", labelKey2num (trueLabel->key));
          break;
        case NE_OP:                        /* left != right : Z=0 */
          emit ("bzr", "!tlabel", labelKey2num (trueLabel->key));
          break;
        case '>':                          /* left >  right : C=1 and Z=0 */
          {
            symbol *falseLabel = newiTempLabel (NULL);

            emit ("bcr", "!tlabel", labelKey2num (falseLabel->key));
            emit ("bzr", "!tlabel", labelKey2num (trueLabel->key));
            emitLbl (falseLabel);
          }
          break;
        case LE_OP:                        /* left <= right : C=0 or Z=1 */
          emit ("bcr", "!tlabel", labelKey2num (trueLabel->key));
          emit ("bzs", "!tlabel", labelKey2num (trueLabel->key));
          break;
        default:
          wassertl (0, "lh5801: unrecognized comparison operator in genCmp");
          break;
        }
    }

  emit ("ldi", "a, 0x00");
  emit ("bch", "!tlabel", labelKey2num (endLabel->key));
  emitLbl (trueLabel);
  emit ("ldi", "a, 0x01");
  emitLbl (endLabel);

  storeA (result->aop, 0);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* emitLongCondBranch - branch to a (possibly distant) target when   */
/* the Z flag does NOT match invMnemonic's own condition             */
/*                                                                    */
/* LH5801's conditional branches (bzr/bzs/...) are S_TYPBRA -- only  */
/* an 8-bit signed relative displacement, nowhere near enough range  */
/* for a branch to an arbitrary user-level iCode label in a large    */
/* function (exactly what produced aslh5801's "Branching Range       */
/* Exceeded" errors compiling device/lib/printf_large.c). jmp is     */
/* S_TYPABS (absolute, unlimited range) but unconditional, so branch */
/* on the *inverse* of the wanted condition (invMnemonic) over a jmp */
/* to the real target: that inverse branch only ever needs to skip   */
/* the 3-byte jmp immediately following it, so it's always in range, */
/* regardless of how far the real target is.                         */
/*-----------------------------------------------------------------*/
static void
emitLongCondBranch (const char *invMnemonic, const symbol *target)
{
  symbol *skip = newiTempLabel (NULL);

  emit (invMnemonic, "!tlabel", labelKey2num (skip->key));
  emit ("jmp", "!tlabel", labelKey2num (target->key));
  emitLbl (skip);
}

/*-----------------------------------------------------------------*/
/* genIfx - generate a conditional branch on a (possibly           */
/* multi-byte) condition operand's truthiness                       */
/*                                                                   */
/* Doesn't need to know anything about how the condition was         */
/* computed (e.g. by genCmp() above, or a plain `if (someInt)`) --   */
/* it just OR-accumulates every byte into A (ORA only needs a memory */
/* operand, matching S_TYPM's addressing modes) and branches on      */
/* whether that's zero.                                              */
/*-----------------------------------------------------------------*/
static void
genIfx (const iCode *ic)
{
  operand *cond = IC_COND (ic);
  int size, i;

  aopOp (cond);
  aopToDir (cond, 0);
  size = cond->aop->size;

  for (i = 0; i < size; i++)
    {
      if (i == 0)
        loadA (cond->aop, 0, size);
      else
        opA ("ora", "ori", cond->aop, i, size);
    }

  freeAsmop (cond);

  if (IC_TRUE (ic) && IC_FALSE (ic))
    {
      emitLongCondBranch ("bzs", IC_TRUE (ic));
      emit ("jmp", "!tlabel", labelKey2num (IC_FALSE (ic)->key));
    }
  else if (IC_FALSE (ic))
    emitLongCondBranch ("bzr", IC_FALSE (ic));
  else if (IC_TRUE (ic))
    emitLongCondBranch ("bzs", IC_TRUE (ic));
}

/*-----------------------------------------------------------------*/
/* genBitwise - generate code for '&' '|' '^'                      */
/*                                                                   */
/* Byte-independent (no carry chain needed, unlike genAddSub()), so  */
/* order doesn't matter. AND/ORA/EOR (S_TYPM) are A-only and support */
/* memory operands but no immediate form at all; ANI/ORI (S_TYPMI)   */
/* and EAI (S_IMMA) are the immediate counterparts (all "A,i" --      */
/* confirmed via the sdas mnemonic table and a standalone assembler   */
/* test), used for AOP_LIT right operands.                           */
/*-----------------------------------------------------------------*/
static void
genBitwise (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  const char *memMnemonic;
  const char *immMnemonic;
  iCode *ifx;

  switch (ic->op)
    {
    case '&':
    case BITWISEAND:  /* BITWISEAND (366): SDCC's separate token from plain
                          '&' used specifically to disambiguate bitwise-AND
                          from address-of at the AST level -- functionally
                          identical by the time it reaches here. */
      memMnemonic = "and"; immMnemonic = "ani"; break;
    case '|': memMnemonic = "ora"; immMnemonic = "ori"; break;
    case '^': memMnemonic = "eor"; immMnemonic = "eai"; break;
    default:
      wassertl (0, "lh5801: unrecognized bitwise operator in genBitwise");
      return;
    }

  aopOp (left);
  aopOp (right);
  aopOp (result);
  aopToDir (right, 0);

  /* `if (x & k)`: BII tests the one byte of x the mask touches -- in
     memory directly, else in A -- and the IFX branches on Z; the result
     is never stored (lh5801_bitTest()). */
  if ((ifx = lh5801_bitTest (ic)))
    {
      int lsize = left->aop->size;
      unsigned char m = 0;
      char buf[128];

      for (i = 0; i < lsize; i++)
        if ((m = litByte (right->aop, i, lsize)))
          break;
      if (left->aop->type == AOP_DIR)
        {
          dirAddr (buf, sizeof (buf), left->aop, i);
          emit ("bii", "(%s), 0x%02x", buf, m);
        }
      else
        {
          loadA (left->aop, i, lsize);
          emit ("bii", "a, #0x%02x", m);
        }
      if (IC_TRUE (ifx))
        emitLongCondBranch ("bzs", IC_TRUE (ifx));
      else
        emitLongCondBranch ("bzr", IC_FALSE (ifx));
      ifx->generated = 1;
      freeAsmop (left);
      freeAsmop (right);
      freeAsmop (result);
      return;
    }

  /* `x &= k` / `x |= k` with x in memory: ANI/ORI (ab),i on each byte the
     mask changes, nothing through A. */
  if (ic->op != '^' && right->aop->type == AOP_LIT && result->aop->type == AOP_DIR &&
      sameStorage (result->aop, left->aop))
    {
      bool isAnd = ic->op != '|';
      char buf[128];

      size = result->aop->size;
      for (i = 0; i < size; i++)
        {
          unsigned char m = litByte (right->aop, i, size);

          if (m == (isAnd ? 0xFF : 0x00))
            continue;
          dirAddr (buf, sizeof (buf), result->aop, i);
          emit (isAnd ? "ani" : "ori", "(%s), 0x%02x", buf, m);
        }
      freeAsmop (left);
      freeAsmop (right);
      freeAsmop (result);
      return;
    }

  /* Same AOP_LIT-size exemption as genAddSub()/genCmp(). */
  if (left->aop->type != AOP_LIT)
    size = left->aop->size;
  else if (right->aop->type != AOP_LIT)
    size = right->aop->size;
  else
    size = (left->aop->size > right->aop->size) ? left->aop->size : right->aop->size;

  wassertl ((left->aop->type == AOP_LIT || left->aop->size == size) &&
            (right->aop->type == AOP_LIT || right->aop->size == size) &&
            result->aop->size == size,
    "lh5801: phase-1 backend requires matching operand/result sizes for &/|/^");

  for (i = 0; i < size; i++)
    {
      loadA (left->aop, i, size);
      opA (memMnemonic, immMnemonic, right->aop, i, size);
      storeA (result->aop, i);
    }

  freeAsmop (left);
  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genShift - generate code for '<<' (LEFT_OP) and '>>' (RIGHT_OP) */
/*                                                                   */
/* Copies left into result, then does `shiftAmount` single-bit       */
/* passes over every byte in place. A compile-time-constant shift    */
/* amount unrolls all of those passes directly (no loop overhead,    */
/* the original phase-1 shape, sufficient for printf_large.c's own    */
/* `n >> 4` / `b << 1`-style usage); a runtime shift amount (needed    */
/* by _fsadd.c's `mant1 >>= expd`, expd not known until runtime)        */
/* instead emits ONE copy of the same single-pass body inside a         */
/* counting loop, decrementing a shared scratch byte (loaded from the    */
/* shift-count operand's low byte -- shift counts are never anywhere      */
/* near 256 in any real program) down to zero.                            */
/*                                                                   */
/* SHL/SHR (inherent, A only) always shift in a fresh 0 bit and      */
/* don't consume carry-in -- confirmed against pc1500emu's opcode    */
/* implementations -- so they're only correct for a single isolated  */
/* byte. Chaining a shift across a multi-byte value instead needs    */
/* ROL/ROR for every byte after the first, since those *do* rotate    */
/* the carry in as the new bit -- giving exactly the "bit shifted     */
/* out of one byte becomes the bit shifted into its neighbor"         */
/* behavior a real multi-byte shift needs. So: first byte processed  */
/* in each pass uses SHL/SHR (correctly injecting a 0 at that end),  */
/* every subsequent byte in that same pass uses ROL/ROR (correctly    */
/* propagating the previous byte's carry-out).                       */
/*                                                                    */
/* Right shifts additionally need to know whether this is a SIGNED    */
/* shift (arithmeticRight): a plain SHR always shifts in 0 (correct    */
/* only for unsigned/logical shifts, or a signed shift of a value       */
/* already known non-negative); a signed shift of a negative value       */
/* must instead shift in copies of the sign bit (arithmetic shift) to     */
/* preserve its value's scaling -- confirmed necessary by _fsadd.c's       */
/* `mant2 >>= expd` on a negative `long` mant2 (both-operands-negative      */
/* addition path): using SHR there corrupted the sign into a huge            */
/* positive value after the very first shifted-out bit. Implemented by       */
/* seeding the carry flag from the CURRENT top bit (via an ANI #0x80 test     */
/* and SEC/REC, reloading the byte before the actual ROR since ANI clobbers   */
/* A) instead of the SHR that would otherwise unconditionally clear it, so     */
/* the same ROR-propagation chain used by every other byte propagates that     */
/* seeded bit in as bit 7 instead of a hardwired 0.                             */
/*-----------------------------------------------------------------*/
static void
emitShiftPass (asmop *resultAop, int size, bool isLeft, bool arithmeticRight)
{
  int i;

  if (isLeft)
    {
      for (i = size - 1; i >= 0; i--)
        {
          loadA (resultAop, i, resultAop->size);
          emit (i == size - 1 ? "shl" : "rol", "");
          storeA (resultAop, i);
        }
    }
  else if (arithmeticRight)
    {
      symbol *nonNegLabel = newiTempLabel (NULL);
      symbol *seededLabel = newiTempLabel (NULL);

      loadA (resultAop, 0, resultAop->size);
      emit ("ani", "a, #0x80");
      emit ("bzs", "!tlabel", labelKey2num (nonNegLabel->key));
      emit ("sec", "");
      emit ("bch", "!tlabel", labelKey2num (seededLabel->key));
      emitLbl (nonNegLabel);
      emit ("rec", "");
      emitLbl (seededLabel);

      for (i = 0; i < size; i++)
        {
          loadA (resultAop, i, resultAop->size);
          emit ("ror", "");
          storeA (resultAop, i);
        }
    }
  else
    {
      for (i = 0; i < size; i++)
        {
          loadA (resultAop, i, resultAop->size);
          emit (i == 0 ? "shr" : "ror", "");
          storeA (resultAop, i);
        }
    }
}

/* `bits` (0-7) single-bit shifts of resultAop in place: unrolled for one
   or two, a counted loop for more. */
static void
emitBitShifts (asmop *resultAop, int size, int bits, bool isLeft, bool arithmeticRight)
{
  if (bits <= 2)
    {
      int pass;

      for (pass = 0; pass < bits; pass++)
        emitShiftPass (resultAop, size, isLeft, arithmeticRight);
    }
  else
    {
      symbol *loopStart = newiTempLabel (NULL);

      emit ("ldi", "a, 0x%02x", bits);
      emit ("sta", "(__lh5801_shift_scratch)");
      emitLbl (loopStart);
      emitShiftPass (resultAop, size, isLeft, arithmeticRight);
      /* sec: see genShift()'s runtime-count loop */
      emit ("lda", "(__lh5801_shift_scratch)");
      emit ("sec", "");
      emit ("sbi", "a, #0x01");
      emit ("sta", "(__lh5801_shift_scratch)");
      emitLongCondBranch ("bzs", loopStart);
    }
}

/*-----------------------------------------------------------------*/
/* genMult - '*' by a constant power of two (the only '*' this port */
/* claims -- see main.c's lh5801_hasNativeMulFor(); every other     */
/* multiply is a support-routine call): the other operand copied,    */
/* widened or narrowed, into the result, then shifted left in place. */
/*-----------------------------------------------------------------*/
static void
genMult (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *var, *lit;
  unsigned long v;
  int size, n = 0, byteShift, i;

  if (IS_OP_LITERAL (right))
    {
      var = left;
      lit = right;
    }
  else
    {
      var = right;
      lit = left;
    }
  v = ulFromVal (OP_VALUE (lit));
  while (v > 1)
    {
      v >>= 1;
      n++;
    }

  aopOp (var);
  aopOp (result);
  size = result->aop->size;

  if (size > var->aop->size)
    genWiden (result, var);
  else
    for (i = 0; i < size; i++)
      {
        loadA (var->aop, var->aop->size - size + i, var->aop->size);
        storeA (result->aop, i);
      }

  byteShift = n / 8 < size ? n / 8 : size;
  if (byteShift)
    {
      for (i = 0; i < size - byteShift; i++)
        {
          loadA (result->aop, i + byteShift, result->aop->size);
          storeA (result->aop, i);
        }
      emit ("ldi", "a, 0x00");
      for (; i < size; i++)
        {
          storeA (result->aop, i);
        }
    }
  if (byteShift < size)
    emitBitShifts (result->aop, size, n % 8, true, false);

  freeAsmop (var);
  freeAsmop (result);
}

static void
genShift (const iCode *ic, bool isLeft)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  bool arithmeticRight;

  aopOp (left);
  aopOp (right);
  aopOp (result);

  size = result->aop->size;
  wassertl (left->aop->type == AOP_LIT || left->aop->size == size,
    "lh5801: phase-1 backend requires matching operand/result sizes for shifts");

  /* A literal being shifted is constant-folded by SDCC's own frontend
     long before this, so it's never actually the value whose sign
     matters here in practice; treat that (theoretical) case as
     unsigned rather than dereferencing a literal's operandType. */
  arithmeticRight = !isLeft && left->aop->type != AOP_LIT &&
                     !SPEC_USIGN (getSpec (operandType (left)));

  if (right->aop->type == AOP_LIT)
    {
      /* A constant shift: whole bytes are moved (byte i of the result is
         byte i +/- k of left -- big-endian, byte 0 = MSB -- and the
         vacated bytes are 0, or the sign for an arithmetic right shift),
         then the remaining 0-7 bits are single-bit passes: unrolled for
         one or two, a counted loop for more. Unrolling every bit, as
         this used to, cost 6-12 bytes per bit per byte. */
      int shiftAmount = (int) ulFromVal (OP_VALUE (right));
      int byteShift = shiftAmount / 8;
      int bits = shiftAmount % 8;

      if (byteShift >= size)
        {
          byteShift = size;
          bits = 0;
        }

      if (isLeft)
        {
          for (i = 0; i < size - byteShift; i++)
            {
              loadA (left->aop, i + byteShift, size);
              storeA (result->aop, i);
            }
          if (byteShift)
            emit ("ldi", "a, 0x00");
          for (; i < size; i++)
            {
              storeA (result->aop, i);
            }
        }
      else
        {
          for (i = size - 1; i >= byteShift; i--)
            {
              loadA (left->aop, i - byteShift, size);
              storeA (result->aop, i);
            }
          if (byteShift)
            {
              if (arithmeticRight)
                {
                  symbol *positive = newiTempLabel (NULL);

                  loadA (left->aop, 0, size);
                  emit ("ani", "a, #0x80");
                  emit ("bzs", "!tlabel", labelKey2num (positive->key));
                  emit ("ldi", "a, 0xff");
                  emitLbl (positive);
                }
              else
                emit ("ldi", "a, 0x00");
            }
          for (; i >= 0; i--)
            {
              storeA (result->aop, i);
            }
        }

      emitBitShifts (result->aop, size, bits, isLeft, arithmeticRight);
    }
  else
    {
      symbol *loopStart = newiTempLabel (NULL);
      symbol *loopEnd = newiTempLabel (NULL);

      /* left -> result (byte copy; left and result may be the same iTemp,
         e.g. for `x <<= n`, in which case this is a no-op each byte). */
      for (i = 0; i < size; i++)
        {
          loadA (left->aop, i, size);
          storeA (result->aop, i);
        }

      loadA (right->aop, right->aop->size - 1, right->aop->size);
      emit ("sta", "(__lh5801_shift_scratch)");

      emitLbl (loopStart);
      emit ("lda", "(__lh5801_shift_scratch)");
      emit ("bzs", "!tlabel", labelKey2num (loopEnd->key));

      emitShiftPass (result->aop, size, isLeft, arithmeticRight);

      /* sbi subtracts *with borrow* (same convention genCmp()/genAddSub()
         already rely on for their own chained subtractions) -- without
         an explicit sec here, this decrement silently inherits whatever
         stale carry emitShiftPass()'s own shr/ror chain just left behind
         (shr/ror are rotate-through-carry, so they always leave one),
         turning "subtract 1" into "subtract 1 or 2" depending on that
         leftover bit and corrupting the whole loop's iteration count. */
      emit ("lda", "(__lh5801_shift_scratch)");
      emit ("sec", "");
      emit ("sbi", "a, #0x01");
      emit ("sta", "(__lh5801_shift_scratch)");
      emit ("jmp", "!tlabel", labelKey2num (loopStart->key));
      emitLbl (loopEnd);
    }

  freeAsmop (left);
  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genGetValueAtAddress - generate code for '*ptr' (pointer          */
/* dereference), printed by piCode() as `result = @[ptr + offset]`  */
/*                                                                   */
/* Confirmed two real shapes of this iCode in practice: `p[1]`-style */
/* runtime array indexing gets pre-decomposed by SDCC's frontend into */
/* a separate pointer-arithmetic iCode ('+') followed by a zero-      */
/* offset dereference of the *result* -- so offset is often already   */
/* zero by the time it gets here. A compile-time-constant struct/     */
/* union-member access (confirmed via printf_large.c's `value.byte[4]`) */
/* instead gets encoded directly as a non-zero literal offset on this */
/* same iCode, no separate arithmetic -- so this does need to handle   */
/* that case for real, not just assert it away. Phase-1 simplification: */
/* the offset must still be a compile-time constant (no variable       */
/* offset support) that fits in 8 bits (every real case seen is a       */
/* small struct-member/array offset, never anywhere close to that       */
/* limit).                                                              */
/*                                                                   */
/* Adds the offset to the pointer value first (low byte, then high   */
/* with the carry from that -- ADI reads the carry flag, so this      */
/* chains exactly like genAddSub()'s multi-byte add), then LIN         */
/* (load-and-increment, one of this port's few genuinely register-    */
/* resident operations -- see the sdas mnemonic table's S_TYPRXYU      */
/* family) walks X across however many bytes the result needs,        */
/* matching our big-endian layout exactly (MSB at the lowest address   */
/* = read first).                                                      */
/*                                                                    */
/* left (the pointer being dereferenced) can itself be a compile-time  */
/* AOP_LIT rather than a real AOP_DIR variable holding a pointer value   */
/* -- same "*(unsigned char *)0x4700" fixed-address idiom as              */
/* genPointerSet()'s own identical gap. Both the pointer and the offset    */
/* are then fully known at compile time, so this just combines them at      */
/* compile time into one constant and loads X directly, no runtime add       */
/* needed at all. */
/*-----------------------------------------------------------------*/
static void
genGetValueAtAddress (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  unsigned long offset = 0;
  char pair;

  aopOp (left);
  aopOp (result);

  wassertl (left->aop->size == 2, "lh5801: pointer dereference requires a 2-byte pointer");

  if (right)
    {
      wassertl (IS_OP_LITERAL (right),
        "lh5801: phase-1 backend only supports a compile-time-constant pointer-dereference offset");
      offset = ulFromVal (OP_VALUE (right));
      wassertl (offset <= 0xFF,
        "lh5801: phase-1 backend only supports an 8-bit pointer-dereference offset");
    }

  size = result->aop->size;

  if (left->aop->type == AOP_IMMD)
    {
      /* A constant address: read it directly, no X at all. */
      for (i = 0; i < size; i++)
        {
          emit ("lda", "(%s + %lu)", left->aop->u.dir, offset + i);
          storeA (result->aop, i);
        }
      freeAsmop (left);
      freeAsmop (result);
      return;
    }

  pair = regPair (left->aop);
  if (pair && !offset && !aopUsesPair (result->aop, pair))
    {
      /* The pointer is already in X, Y or U: read through it there. */
      for (i = 0; i < size; i++)
        {
          emit ("lin", "%c", pair);
          storeA (result->aop, i);
        }
      if (aopSurvives (ic, left->aop))
        for (i = 0; i < size; i++)
          emit ("dec", "%c", pair);
      freeAsmop (left);
      freeAsmop (result);
      return;
    }

  loadPtrX (left->aop, offset);

  for (i = 0; i < size; i++)
    {
      emit ("lin", "x");
      storeA (result->aop, i);
    }

  freeAsmop (left);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genUnaryMinus - generate code for unary '-' (two's-complement    */
/* negation)                                                          */
/*                                                                   */
/* Just genAddSub()'s subtraction chain with a literal 0 as the left */
/* operand -- 0 - x is exactly two's-complement negation. */
/*-----------------------------------------------------------------*/
static void
genUnaryMinus (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  /* newiCode(UNARYMINUS, op, NULL) (SDCCicode.c) -- the operand is
     IC_LEFT, not IC_RIGHT (which is always NULL for a unary op and
     will crash aopOp() if read here by mistake). */
  operand *right = IC_LEFT (ic);
  int size, i;

  aopOp (right);
  aopOp (result);
  aopToDir (right, 0);

  size = result->aop->size;
  wassertl (right->aop->type == AOP_LIT || right->aop->size == size,
    "lh5801: phase-1 backend requires matching operand/result sizes for unary '-'");

  emit ("sec", "");

  for (i = size - 1; i >= 0; i--)
    {
      emit ("ldi", "a, 0x00");
      opA ("sbc", "sbi", right->aop, i, size);
      storeA (result->aop, i);
    }

  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genNot - generate code for '!' (logical NOT)                    */
/*                                                                   */
/* Same "OR every byte together" truthiness test genIfx() uses, just */
/* materializing the 0/1 result into memory instead of branching on  */
/* it directly. */
/*-----------------------------------------------------------------*/
static void
genNot (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  /* newiCode('!', operand, 0) (SDCCicode.c) -- same IC_LEFT-not-IC_RIGHT
     convention as UNARYMINUS above. */
  operand *right = IC_LEFT (ic);
  int size, i;
  /* newiTempLabel()/!tlabel -- see the identical comment in genCmp()
     above for why a plain fixed-name label here would break aslh5801's
     reusable-local-symbol scoping for every numbered label elsewhere
     in this function. */
  symbol *trueLabel = newiTempLabel (NULL);
  symbol *endLabel = newiTempLabel (NULL);

  aopOp (right);
  aopOp (result);
  aopToDir (right, 0);
  wassertl (result->aop->size == 1, "lh5801: '!' result must be 1 byte");

  size = right->aop->size;

  for (i = 0; i < size; i++)
    {
      if (i == 0)
        loadA (right->aop, 0, size);
      else
        opA ("ora", "ori", right->aop, i, size);
    }

  freeAsmop (right);

  emit ("bzs", "!tlabel", labelKey2num (trueLabel->key));
  emit ("ldi", "a, 0x00");
  emit ("bch", "!tlabel", labelKey2num (endLabel->key));
  emitLbl (trueLabel);
  emit ("ldi", "a, 0x01");
  emitLbl (endLabel);

  storeA (result->aop, 0);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* dropReloads - remove "lda (m)" right after "sta (m)"             */
/*                                                                   */
/* Every gen*() here stores its result to memory and the next one     */
/* loads its operand back, so this pair is everywhere (it was ~180     */
/* times in a 600-line program). A still holds the value; the only      */
/* thing the LDA adds is Z set from it, so the pair stays whenever a     */
/* conditional branch follows -- the instruction that produced A might    */
/* not have set Z from it (a call, LDA XH, ...). Labels, comments and      */
/* inline asm are left alone, and so are register-indirect operands.       */
/*-----------------------------------------------------------------*/
static void
dropReloads (lineNode *head)
{
  lineNode *ln;

  for (ln = head; ln && ln->next; ln = ln->next)
    {
      lineNode *nx = ln->next;
      const char *br;

      if (!ln->line || !nx->line || ln->isInline || nx->isInline || ln->isLabel ||
          nx->isLabel || ln->isComment || nx->isComment)
        continue;
      if (strncmp (ln->line, "sta\t(", 5) || strncmp (nx->line, "lda\t(", 5) ||
          strcmp (ln->line + 3, nx->line + 3) ||
          !strcmp (ln->line + 4, "(x)") || !strcmp (ln->line + 4, "(y)") ||
          !strcmp (ln->line + 4, "(u)"))
        continue;
      br = nx->next ? nx->next->line : NULL;
      if (br && br[0] == 'b' && br[1] && strchr ("zchv", br[1]) && (br[2] == 's' || br[2] == 'r'))
        continue;

      ln->next = nx->next;
      if (nx->next)
        nx->next->prev = ln;
      if (genLine.lineCurr == nx)
        genLine.lineCurr = ln;
      Safe_free (nx->line);
      if (nx->aln)
        Safe_free (nx->aln);
      Safe_free (nx);
    }
}

/*-----------------------------------------------------------------*/
/* genIC - generate code for one iCode                              */
/*-----------------------------------------------------------------*/
static void
genIC (iCode *ic)
{
  /* A rematerializable result (see AOP_IMMD) is never stored anywhere:
     every use of it rebuilds the constant address itself. */
  if ((ic->op == ADDRESS_OF || ic->op == '+' || ic->op == '-' ||
       ic->op == CAST || ic->op == '=') &&
      !POINTER_SET (ic) && isRemat (IC_RESULT (ic)))
    return;

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
      if (POINTER_SET (ic))
        genPointerSet (ic);
      else
        genAssign (ic);
      break;
    case IPUSH:
      genIpush (ic);
      break;
    case CALL:
    case PCALL:
      genCall (ic);
      break;
    case ADDRESS_OF:
      genAddrOf (ic);
      break;
    case CAST:
      genCast (ic);
      break;
    case '+':
      genAddSub (ic, false);
      break;
    case '-':
      genAddSub (ic, true);
      break;
    case '*':
      genMult (ic);
      break;
    case '>':
    case '<':
    case LE_OP:
    case GE_OP:
    case EQ_OP:
    case NE_OP:
      genCmp (ic, ifxForOp (IC_RESULT (ic), ic));
      break;
    case IFX:
      genIfx (ic);
      break;
    case '&':
    case BITWISEAND:
    case '|':
    case '^':
      genBitwise (ic);
      break;
    case LEFT_OP:
      genShift (ic, true);
      break;
    case RIGHT_OP:
      genShift (ic, false);
      break;
    case GET_VALUE_AT_ADDRESS:
      genGetValueAtAddress (ic);
      break;
    case UNARYMINUS:
      genUnaryMinus (ic);
      break;
    case '!':
      genNot (ic);
      break;
    default:
      /* Print the actual iCode (not just its raw numeric op) to make
         the next gap easy to identify without needing a debugger --
         SDCCy.h has the numeric-to-name mapping if piCode's own
         textual form isn't enough. */
      fprintf (stderr, "lh5801: unimplemented iCode (op=%d): ", ic->op);
      piCode (ic, stderr);
      wassertl (0, "lh5801: iCode operator not yet implemented in the phase-1 backend");
      break;
    }
}

/* The register allocator's price for code generation of one iCode with
   its operands' candidate registers (ralloc2.cc's instruction_cost()):
   its size in bytes, without emitting anything. */
float
drylh5801iCode (iCode *ic)
{
  /* The labels a dry run makes (newiTempLabel()) are never emitted, so
     their numbers are given back: the allocator runs the code generator
     thousands of times, and this port never resets labelKey (main.c), so
     the numbers would soon pass 65535 -- past what aslh5801's NNNNN$
     local labels can hold. */
  int savedLabelKey = labelKey;

  regalloc_dry_run = true;
  dryCost = 0;
  genIC (ic);
  regalloc_dry_run = false;
  labelKey = savedLabelKey;
  return dryCost;
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
      ic->generated = 1;
      genIC (ic);
    }

  /* Flush the accumulated line list into the output buffer -- every other
     port's gen*Code() does this as its last act (e.g. stm8/gen.c,
     mcs51/gen.c); nothing shared calls printLine() for us.

     destroy_line_list() (SDCCgen.c) afterward is essential, not optional
     cleanup: genLH5801Code() is called once per function (via
     lh5801_assignRegisters(), ralloc.c), and printLine() only reads
     genLine.lineHead -- it never clears it. Without destroying the list
     here, the next function's call would keep appending to this one's
     already-printed lines, and printLine() would emit everything *before*
     the current function all over again alongside it (caught via a real
     multi-function test: sdas rejected the resulting duplicate labels).
     stm8/gen.c's genStm8Code() does the same destroy_line_list() call for
     the same reason. */
  dropReloads (genLine.lineHead);
  printLine (genLine.lineHead, codeOutBuf);
  destroy_line_list ();
}

/*-----------------------------------------------------------------*/
/* genLH5801OrphanedLabels - emit bare definitions for any basic    */
/* block dropped from genLH5801Code()'s own chain                   */
/*                                                                   */
/* iCodeFromeBBlock() (SDCCBBlock.c) silently skips any block marked */
/* noPath (SDCC's own control-flow analysis proved it unreachable),  */
/* *including* its label -- confirmed via device/lib/printf_large.c: */
/* its non-__SDCC_STACK_AUTO code path (the one this port's           */
/* options.stackAuto=0 always selects, far less exercised than the   */
/* reentrant path most other ports use) has a block real ROM-running */
/* control flow never reaches, so SDCC correctly determines it's      */
/* dead. But *this port's own* IFX/GOTO codegen still emitted a       */
/* branch instruction targeting that label from a still-live block    */
/* earlier -- iCodeFromeBBlock() dropping the label along with the    */
/* rest of the dead block left that branch with nowhere to go,        */
/* confirmed to be exactly the cause of the assembler's "undefined     */
/* symbol"/"branches must target a symbol in the same area" errors     */
/* (via a debug trace of every LABEL/GOTO/IFX in the chain, cross-      */
/* checked against genLabel() never actually being called for that     */
/* key). This just makes sure the label exists somewhere so the         */
/* assembler is happy, even though the code at it is genuinely dead --  */
/* the branch instruction pointing at it never executes either.         */
/*-----------------------------------------------------------------*/
void
genLH5801OrphanedLabels (eBBlock **ebbs, int count)
{
  int i;
  bool anyEmitted = false;

  for (i = 0; i < count; i++)
    {
      symbol *lbl = ebbs[i]->entryLabel;

      if (!ebbs[i]->noPath || !lbl || lbl == entryLabel || lbl == returnLabel)
        continue;

      anyEmitted = true;
      initGenLineElement ();
      emitLbl (lbl);
    }

  if (anyEmitted)
    {
      printLine (genLine.lineHead, codeOutBuf);
      destroy_line_list ();
    }
}
