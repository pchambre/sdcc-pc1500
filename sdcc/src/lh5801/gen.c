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

/* Loads byte `offset` of `aop` (LIT, DIR, or FUNC) into A -- the same
   check genAssign()/genReturn()/genIpush() each used to do inline,
   factored out since genAddSub()/genCmp() below both need it too.
   AOP_FUNC is 2-byte-only (a flat address in this port); offset 0 is
   the high byte (">"), offset 1 the low byte ("<"), matching
   genAddrOf()'s own convention exactly. */
static void
loadByteToA (const asmop *aop, int offset, int size)
{
  if (aop->type == AOP_LIT)
    emitcode ("ldi", "a, 0x%02x", litByte (aop, offset, size));
  else if (aop->type == AOP_FUNC)
    {
      wassertl (size == 2, "lh5801: a function reference must be a 2-byte pointer");
      emitcode ("ldi", "a, %s%s", offset == 0 ? ">" : "<", aop->u.dir);
    }
  else
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), aop, offset);
      emitcode ("lda", "(%s)", buf);
    }
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
  fprintf (of, "\t.area CODE\n");
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

  emitcode ("lda", "(__lh5801_frame_ptr + 1)");  /* low byte */
  emitcode ("rec", "");
  emitcode ("adi", "a, #0x%02lx", (unsigned long) stackOffset);
  emitcode ("sta", "xl");

  emitcode ("lda", "(__lh5801_frame_ptr)");       /* high byte */
  emitcode ("adi", "a, #0x00");                    /* propagate carry-out */
  emitcode ("sta", "xh");
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

  emitcode (";", "-----------------------------------------");
  emitcode (";", " function %s", sym->name);
  emitcode (";", "-----------------------------------------");

  emitcode ("", "%s:", sym->rname);
  genLine.lineCurr->isLabel = 1;

  for (arg = FUNC_ARGS (sym->type); arg; arg = arg->next)
    if (arg->sym && arg->sym->onStack)
      {
        anyOnStack = true;
        break;
      }

  if (!anyOnStack)
    return;

  emitcode ("ldx", "s");
  emitcode ("lda", "xh");
  emitcode ("sta", "(__lh5801_frame_ptr)");
  emitcode ("lda", "xl");
  emitcode ("sta", "(__lh5801_frame_ptr + 1)");

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
          emitcode ("lin", "x");
          emitcode ("sta", "(%s + %d)", shadow->rname, j);
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
  emitcode ("rtn", "");
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

      wassertl (left->aop->size <= 2,
        "lh5801: phase-1 backend only supports 0-, 1-, or 2-byte return values");

      if (left->aop->size == 1)
        {
          if (left->aop->type == AOP_LIT)
            emitcode ("ldi", "a, 0x%02x", litByte (left->aop, 0, 1));
          else
            emitcode ("lda", "(%s)", left->aop->u.dir);
        }
      else if (left->aop->size == 2)
        {
          if (left->aop->type == AOP_LIT)
            {
              emitcode ("ldi", "xh, 0x%02x", litByte (left->aop, 0, 2));
              emitcode ("ldi", "xl, 0x%02x", litByte (left->aop, 1, 2));
            }
          else
            {
              char buf[128];

              dirAddr (buf, sizeof (buf), left->aop, 0);
              emitcode ("lda", "(%s)", buf);
              emitcode ("sta", "xh");

              dirAddr (buf, sizeof (buf), left->aop, 1);
              emitcode ("lda", "(%s)", buf);
              emitcode ("sta", "xl");
            }
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
  /* jmp (S_TYPABS, absolute 16-bit target), not bch (S_TYPBRA, an 8-bit
     signed relative displacement) -- a goto's target can be arbitrarily
     far away in a large function (a loop back-edge, a distant switch
     case, ...), and bch's limited range is exactly what produced
     aslh5801's "Branching Range Exceeded" errors compiling
     device/lib/printf_large.c. jmp has no such limit. */
  emitcode ("jmp", "!tlabel", labelKey2num (IC_LABEL (ic)->key));
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
      loadByteToA (left->aop, i, size);
      emitcode ("psh", "a");
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
  char buf[128];

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
     stored directly into the result instead of dereferenced further. */
  if (sym->onStack)
    {
      wassertl (offset >= 0,
        "lh5801: ADDRESS_OF a stack symbol needs a non-negative offset");

      loadStackAddrToX (sym->stack + 1 + offset);

      aopOp (result);
      wassertl (result->aop->size == 2, "lh5801: ADDRESS_OF result must be a 2-byte pointer");

      emitcode ("lda", "xh");
      dirAddr (buf, sizeof (buf), result->aop, 0);
      emitcode ("sta", "(%s)", buf);

      emitcode ("lda", "xl");
      dirAddr (buf, sizeof (buf), result->aop, 1);
      emitcode ("sta", "(%s)", buf);

      freeAsmop (result);
      return;
    }

  if (offset)
    snprintf (sym_expr, sizeof (sym_expr), "(%s + %ld)", sym->rname, offset);
  else
    snprintf (sym_expr, sizeof (sym_expr), "%s", sym->rname);

  aopOp (result);
  wassertl (result->aop->size == 2, "lh5801: ADDRESS_OF result must be a 2-byte pointer");

  emitcode ("ldi", "a, >%s", sym_expr);
  dirAddr (buf, sizeof (buf), result->aop, 0);
  emitcode ("sta", "(%s)", buf);

  emitcode ("ldi", "a, <%s", sym_expr);
  dirAddr (buf, sizeof (buf), result->aop, 1);
  emitcode ("sta", "(%s)", buf);

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
/* Widening (result bigger than source) is a phase-1 simplification: */
/* always zero-extends. Correct for the unsigned char/long casts     */
/* printf_large.c's own code needs (confirmed -- e.g. widening a      */
/* `value.byte[4]` unsigned char into an unsigned long accumulator); */
/* wrong for a genuinely negative signed source, which would need     */
/* its sign bit replicated into every new high-order byte instead.   */
/* Not implemented -- no signed-widening case has come up yet.       */
/*-----------------------------------------------------------------*/
static void
genCast (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  int resultSize, rightSize, i;
  char buf[128];

  aopOp (right);
  aopOp (result);

  resultSize = result->aop->size;
  rightSize = right->aop->size;

  if (resultSize <= rightSize)
    {
      int srcOffset = rightSize - resultSize;

      for (i = 0; i < resultSize; i++)
        {
          loadByteToA (right->aop, srcOffset + i, rightSize);
          dirAddr (buf, sizeof (buf), result->aop, i);
          emitcode ("sta", "(%s)", buf);
        }
    }
  else
    {
      int extra = resultSize - rightSize;

      for (i = 0; i < extra; i++)
        {
          emitcode ("ldi", "a, 0x00");
          dirAddr (buf, sizeof (buf), result->aop, i);
          emitcode ("sta", "(%s)", buf);
        }

      for (i = 0; i < rightSize; i++)
        {
          loadByteToA (right->aop, i, rightSize);
          dirAddr (buf, sizeof (buf), result->aop, extra + i);
          emitcode ("sta", "(%s)", buf);
        }
    }

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
      char buf[128];

      if (!pcallTrampolineEmitted)
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
          emitcode ("bch", "__lh5801_pcall_trampoline_skip");
          emitcode ("", "__lh5801_pcall_trampoline:");
          genLine.lineCurr->isLabel = 1;
          emitcode ("jmp", "0x0000");
          emitcode ("", "__lh5801_pcall_trampoline_skip:");
          genLine.lineCurr->isLabel = 1;
        }

      aopOp (left);
      wassertl (left->aop->size == 2, "lh5801: PCALL target must be a 2-byte function pointer");

      dirAddr (buf, sizeof (buf), left->aop, 0);
      emitcode ("lda", "(%s)", buf);
      emitcode ("sta", "(__lh5801_pcall_trampoline + 1)");

      dirAddr (buf, sizeof (buf), left->aop, 1);
      emitcode ("lda", "(%s)", buf);
      emitcode ("sta", "(__lh5801_pcall_trampoline + 2)");

      freeAsmop (left);

      emitcode ("sjp", "__lh5801_pcall_trampoline");
    }
  else
    {
      symbol *sym = OP_SYMBOL (IC_LEFT (ic));

      emitcode ("sjp", "%s", sym->rname[0] ? sym->rname : sym->name);
    }

  while (parmBytes >= 2)
    {
      emitcode ("pop", "x");
      parmBytes -= 2;
    }
  if (parmBytes)
    emitcode ("pop", "a");

  if (IC_RESULT (ic))
    {
      operand *result = IC_RESULT (ic);
      int size;

      aopOp (result);
      size = result->aop->size;

      wassertl (size <= 2,
        "lh5801: phase-1 backend only supports 0-, 1-, or 2-byte call results");

      /* Matches genReturn()'s return-value convention: 1 byte in A,
         2 bytes in X (XH = most-significant byte). */
      if (size == 1)
        emitcode ("sta", "(%s)", result->aop->u.dir);
      else if (size == 2)
        {
          char buf[128];

          emitcode ("lda", "xh");
          dirAddr (buf, sizeof (buf), result->aop, 0);
          emitcode ("sta", "(%s)", buf);

          emitcode ("lda", "xl");
          dirAddr (buf, sizeof (buf), result->aop, 1);
          emitcode ("sta", "(%s)", buf);
        }

      freeAsmop (result);
    }
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

      loadByteToA (right->aop, i, size);

      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
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

  emitcode (isSub ? "sec" : "rec", "");

  for (i = size - 1; i >= 0; i--)
    {
      char buf[128];

      if (i < leftExtra)
        emitcode ("ldi", "a, 0x00");
      else
        loadByteToA (left->aop, i - leftExtra, left->aop->size);

      if (right->aop->type == AOP_LIT)
        emitcode (isSub ? "sbi" : "adi", "a, #0x%02x", litByte (right->aop, i, size));
      else if (i < rightExtra)
        /* No real memory address holds this zero-extension byte -- add/
           subtract a literal 0 instead, still propagating the carry
           chain exactly like a real byte would. */
        emitcode (isSub ? "sbi" : "adi", "a, #0x00");
      else
        {
          dirAddr (buf, sizeof (buf), right->aop, i - rightExtra);
          emitcode (isSub ? "sbc" : "adc", "(%s)", buf);
        }

      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
    }

  freeAsmop (left);
  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genCmp - generate code for '>' '<' LE_OP GE_OP EQ_OP NE_OP       */
/*                                                                   */
/* Phase-1 simplification: always compares as if unsigned, even for */
/* signed operand types -- the LH5801 has no sign/negative flag at   */
/* all (confirmed against pc1500emu's Flags struct: only c/v/z/h),   */
/* so the usual "N xor V" signed-comparison trick isn't directly     */
/* available, and implementing it via manual top-bit extraction      */
/* isn't done yet. Correct for same-sign operands (the overwhelming  */
/* common case, including everything printf_large.c's own comparisons */
/* need -- they're all unsigned char/long), wrong for e.g. comparing */
/* a negative int against a positive one.                            */
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
/* scratch byte is 0 iff left==right.                                */
/*-----------------------------------------------------------------*/
static void
genCmp (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
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

  /* Same AOP_LIT-size exemption as genAddSub() -- a literal's own aop
     size can be smaller than the real operation width (SDCC sizes a
     literal to the smallest type that fits its value, not to whatever
     it's being compared against), but litByte() extracts the correct
     byte regardless. Prefer a real (AOP_DIR) operand's size as the
     canonical width; only fall back to comparing two literals' sizes
     directly (a case the constant folder would normally have already
     eliminated) if both sides are literal. */
  if (left->aop->type != AOP_LIT)
    size = left->aop->size;
  else if (right->aop->type != AOP_LIT)
    size = right->aop->size;
  else
    size = (left->aop->size > right->aop->size) ? left->aop->size : right->aop->size;

  wassertl ((left->aop->type == AOP_LIT || left->aop->size == size) &&
            (right->aop->type == AOP_LIT || right->aop->size == size),
    "lh5801: phase-1 backend requires matching operand sizes for comparisons");
  wassertl (result->aop->size == 1, "lh5801: comparison result must be 1 byte");

  emitcode ("ldi", "a, 0x00");
  emitcode ("sta", "(__lh5801_cmp_scratch)");

  emitcode ("sec", "");

  for (i = size - 1; i >= 0; i--)
    {
      char buf[128];

      loadByteToA (left->aop, i, size);

      if (right->aop->type == AOP_LIT)
        emitcode ("sbi", "a, #0x%02x", litByte (right->aop, i, size));
      else
        {
          dirAddr (buf, sizeof (buf), right->aop, i);
          emitcode ("sbc", "(%s)", buf);
        }

      emitcode ("ora", "(__lh5801_cmp_scratch)");
      emitcode ("sta", "(__lh5801_cmp_scratch)");
    }

  freeAsmop (left);
  freeAsmop (right);

  /* C=1 (no borrow) means left>=right; Z (from the last ora/sta above)
     means left==right. Each case below jumps to trueLabel when its
     condition holds, falls through to "store 0" otherwise. */
  switch (ic->op)
    {
    case '<':                          /* left <  right : C=0 */
      emitcode ("bcr", "!tlabel", labelKey2num (trueLabel->key));
      break;
    case GE_OP:                        /* left >= right : C=1 */
      emitcode ("bcs", "!tlabel", labelKey2num (trueLabel->key));
      break;
    case EQ_OP:                        /* left == right : Z=1 */
      emitcode ("bzs", "!tlabel", labelKey2num (trueLabel->key));
      break;
    case NE_OP:                        /* left != right : Z=0 */
      emitcode ("bzr", "!tlabel", labelKey2num (trueLabel->key));
      break;
    case '>':                          /* left >  right : C=1 and Z=0 */
      {
        symbol *falseLabel = newiTempLabel (NULL);

        emitcode ("bcr", "!tlabel", labelKey2num (falseLabel->key));
        emitcode ("bzr", "!tlabel", labelKey2num (trueLabel->key));
        emitLabel (falseLabel);
      }
      break;
    case LE_OP:                        /* left <= right : C=0 or Z=1 */
      emitcode ("bcr", "!tlabel", labelKey2num (trueLabel->key));
      emitcode ("bzs", "!tlabel", labelKey2num (trueLabel->key));
      break;
    default:
      wassertl (0, "lh5801: unrecognized comparison operator in genCmp");
      break;
    }

  emitcode ("ldi", "a, 0x00");
  emitcode ("bch", "!tlabel", labelKey2num (endLabel->key));
  emitLabel (trueLabel);
  emitcode ("ldi", "a, 0x01");
  emitLabel (endLabel);

  emitcode ("sta", "(%s)", result->aop->u.dir);
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

  emitcode (invMnemonic, "!tlabel", labelKey2num (skip->key));
  emitcode ("jmp", "!tlabel", labelKey2num (target->key));
  emitLabel (skip);
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
  size = cond->aop->size;

  for (i = 0; i < size; i++)
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), cond->aop, i);
      emitcode (i == 0 ? "lda" : "ora", "(%s)", buf);
    }

  freeAsmop (cond);

  if (IC_TRUE (ic) && IC_FALSE (ic))
    {
      emitLongCondBranch ("bzs", IC_TRUE (ic));
      emitcode ("jmp", "!tlabel", labelKey2num (IC_FALSE (ic)->key));
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
      char buf[128];

      loadByteToA (left->aop, i, size);

      if (right->aop->type == AOP_LIT)
        emitcode (immMnemonic, "a, #0x%02x", litByte (right->aop, i, size));
      else
        {
          dirAddr (buf, sizeof (buf), right->aop, i);
          emitcode (memMnemonic, "(%s)", buf);
        }

      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
    }

  freeAsmop (left);
  freeAsmop (right);
  freeAsmop (result);
}

/*-----------------------------------------------------------------*/
/* genShift - generate code for '<<' (LEFT_OP) and '>>' (RIGHT_OP) */
/*                                                                   */
/* Phase-1 simplification: the shift amount must be a compile-time  */
/* constant (a variable shift amount would need a runtime counting  */
/* loop -- not implemented, not needed by printf_large.c's own       */
/* `n >> 4` / `b << 1`-style usage). Copies left into result, then    */
/* does `shiftAmount` single-bit passes over every byte in place.    */
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
/*-----------------------------------------------------------------*/
static void
genShift (const iCode *ic, bool isLeft)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i, pass, shiftAmount;
  char buf[128];

  aopOp (left);
  aopOp (right);
  aopOp (result);

  size = result->aop->size;
  wassertl (left->aop->type == AOP_LIT || left->aop->size == size,
    "lh5801: phase-1 backend requires matching operand/result sizes for shifts");
  wassertl (right->aop->type == AOP_LIT,
    "lh5801: phase-1 backend only supports a compile-time-constant shift amount");

  shiftAmount = (int) ulFromVal (OP_VALUE (right));

  /* left -> result (byte copy; left and result may be the same iTemp,
     e.g. for `x <<= n`, in which case this is a no-op each byte). */
  for (i = 0; i < size; i++)
    {
      loadByteToA (left->aop, i, size);
      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
    }

  for (pass = 0; pass < shiftAmount; pass++)
    {
      if (isLeft)
        {
          for (i = size - 1; i >= 0; i--)
            {
              dirAddr (buf, sizeof (buf), result->aop, i);
              emitcode ("lda", "(%s)", buf);
              emitcode (i == size - 1 ? "shl" : "rol", "");
              emitcode ("sta", "(%s)", buf);
            }
        }
      else
        {
          for (i = 0; i < size; i++)
            {
              dirAddr (buf, sizeof (buf), result->aop, i);
              emitcode ("lda", "(%s)", buf);
              emitcode (i == 0 ? "shr" : "ror", "");
              emitcode ("sta", "(%s)", buf);
            }
        }
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
/*-----------------------------------------------------------------*/
static void
genGetValueAtAddress (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  int size, i;
  char buf[128];
  unsigned long offset = 0;

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

  dirAddr (buf, sizeof (buf), left->aop, 1);  /* low byte */
  emitcode ("lda", "(%s)", buf);
  if (offset)
    {
      emitcode ("rec", "");
      emitcode ("adi", "a, #0x%02lx", offset);
    }
  emitcode ("sta", "xl");

  dirAddr (buf, sizeof (buf), left->aop, 0);  /* high byte */
  emitcode ("lda", "(%s)", buf);
  if (offset)
    emitcode ("adi", "a, #0x00");  /* propagate the low byte's carry-out */
  emitcode ("sta", "xh");

  for (i = 0; i < size; i++)
    {
      emitcode ("lin", "x");
      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
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

  size = result->aop->size;
  wassertl (right->aop->type == AOP_LIT || right->aop->size == size,
    "lh5801: phase-1 backend requires matching operand/result sizes for unary '-'");

  emitcode ("sec", "");

  for (i = size - 1; i >= 0; i--)
    {
      char buf[128];

      emitcode ("ldi", "a, 0x00");

      if (right->aop->type == AOP_LIT)
        emitcode ("sbi", "a, #0x%02x", litByte (right->aop, i, size));
      else
        {
          dirAddr (buf, sizeof (buf), right->aop, i);
          emitcode ("sbc", "(%s)", buf);
        }

      dirAddr (buf, sizeof (buf), result->aop, i);
      emitcode ("sta", "(%s)", buf);
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
  wassertl (result->aop->size == 1, "lh5801: '!' result must be 1 byte");

  size = right->aop->size;

  for (i = 0; i < size; i++)
    {
      char buf[128];

      dirAddr (buf, sizeof (buf), right->aop, i);
      emitcode (i == 0 ? "lda" : "ora", "(%s)", buf);
    }

  freeAsmop (right);

  emitcode ("bzs", "!tlabel", labelKey2num (trueLabel->key));
  emitcode ("ldi", "a, 0x00");
  emitcode ("bch", "!tlabel", labelKey2num (endLabel->key));
  emitLabel (trueLabel);
  emitcode ("ldi", "a, 0x01");
  emitLabel (endLabel);

  emitcode ("sta", "(%s)", result->aop->u.dir);
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
      ic->generated = 1;

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
        case '>':
        case '<':
        case LE_OP:
        case GE_OP:
        case EQ_OP:
        case NE_OP:
          genCmp (ic);
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
      emitLabel (lbl);
    }

  if (anyEmitted)
    {
      printLine (genLine.lineHead, codeOutBuf);
      destroy_line_list ();
    }
}
