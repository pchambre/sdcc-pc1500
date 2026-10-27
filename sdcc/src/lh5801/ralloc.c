/*-------------------------------------------------------------------------
  ralloc.c - register allocation for the LH5801 port.

  Paul Chambre, 2026

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  Phase 1: no real register allocation at all (see ralloc.h). This file's
  only job is to make sure every compiler-generated temporary (iTemp) that
  needs to survive between iCodes has a valid memory address before gen.c
  runs, then hand off to code generation.

  With options.stackAuto = 0 (set in main.c), allocLocal() (SDCCmem.c)
  routes S_AUTO symbols to static "data" storage rather than a real stack
  frame -- so giving an iTemp a spill location this way is exactly the
  same mechanism true C local variables already use in this mode, not a
  separate stack-allocation path we'd have to implement ourselves.
-------------------------------------------------------------------------*/

#include "ralloc.h"
#include "gen.h"

#include "dbuf_string.h"

reg_info lh5801_regs[] =
{
  {0, A_IDX,  "a"},
  {0, SP_IDX, "s"},
};

static int slocNum = 0;

/*-----------------------------------------------------------------*/
/* createStaticSpil - give an iTemp a static memory location       */
/*-----------------------------------------------------------------*/
static void
createStaticSpil (symbol *sym)
{
  symbol *sloc;
  struct dbuf_s dbuf;

  dbuf_init (&dbuf, 32);
  dbuf_printf (&dbuf, "sloc%d", slocNum++);
  sloc = newiTemp (dbuf_c_str (&dbuf));
  dbuf_destroy (&dbuf);

  sloc->type = copyLinkChain (sym->type);
  sloc->etype = getSpec (sloc->type);
  SPEC_SCLS (sloc->etype) = S_AUTO;
  SPEC_EXTR (sloc->etype) = 0;
  SPEC_STAT (sloc->etype) = 0;
  SPEC_VOLATILE (sloc->etype) = 0;

  {
    /* allocLocal() (SDCCmem.c) routes an S_AUTO local onto the real
       hardware stack whenever "options.stackAuto || reentrant" --
       "reentrant" is a global counter (SDCCast.c's createFunction(),
       "reentrant++"/"reentrant--") that stays >0 for the ENTIRE
       compilation of any __reentrant-qualified function's body,
       including this exact register-allocation phase (confirmed
       directly: a plain `int c; c = g; return c;` local inside a
       __reentrant function silently got onStack=1/ocls=STACK here,
       and SDCCglue.c then never emitted its storage at all -- since a
       properly-implemented reentrant port would give it a *real*
       stack-relative address instead, which is exactly what this
       one-instruction memmap check assumes). Every spill location this
       phase-1 backend creates must be a real, fixed, static address --
       that's the entire premise of a backend with no register
       allocator and no general stack-relative addressing (only named
       parameters and __va_start get that, and only via their own
       dedicated mechanism in gen.c's genFunction()/genAddrOf()) -- so
       defeat the check just for this one call. */
    int savedReentrant = reentrant;

    reentrant = 0;
    allocLocal (sloc);
    reentrant = savedReentrant;
  }

  sloc->isref = 1;

  /* SDCCglue.c only emits storage for a level>0 (local) symbol if
     allocreq is set (see emitRegularMap()/emitStaticSeg()) -- normally
     set by a port's real register allocator when it decides a symbol
     needs a memory location after all. We have no real register
     allocator (everything is spilt, unconditionally), so nothing else
     ever sets it; without this, sdas fails with "undefined symbol" for
     every spill location actually used across a function call or
     assignment (confirmed via `int f(void) { volatile int y; int x;
     x = y; return x; }`-style test cases with a real, unoptimized-away
     local). */
  sloc->allocreq = 1;

  sym->usl.spillLoc = sloc;
  sym->isspilt = 1;
}

/*-----------------------------------------------------------------*/
/* packRegsForAssign - turn "iTempNN = a op b ... TrueSym := iTempNN"*/
/* into "TrueSym = a op b"                                           */
/*                                                                   */
/* hc08's (src/hc08/ralloc.c), unchanged in substance. Without it,   */
/* every `w += z;` computed into a spill location and then copied     */
/* that into w a byte at a time -- 6 bytes of code per byte of the     */
/* value, for nothing. Every gen*() here already reads each source      */
/* byte before writing the same result byte, so a result that is also   */
/* an operand (`w = w + z`) needs no special care.                      */
/*-----------------------------------------------------------------*/
static int
packRegsForAssign (iCode *ic, eBBlock *ebp)
{
  iCode *dic, *sic;

  if (!IS_ITEMP (IC_RIGHT (ic)) ||
      OP_SYMBOL (IC_RIGHT (ic))->isind ||
      OP_LIVETO (IC_RIGHT (ic)) > ic->seq)
    return 0;

  /* A narrowing (or widening) '=' stays: most gen*() here need their
     result as wide as their operands, unlike hc08's. */
  if (getSize (operandType (IC_RESULT (ic))) != getSize (operandType (IC_RIGHT (ic))))
    return 0;

  /* find the definition of iTempNN scanning backwards if we find
     a use of the true symbol before we find the definition then
     we cannot */
  for (dic = ic->prev; dic; dic = dic->prev)
    {
      int crossedCall = 0;

      /* We can pack across a function call only if it's a local
         variable or our parameter. Never pack global variables
         or parameters to a function we call. */
      if ((dic->op == CALL || dic->op == PCALL) &&
          !OP_SYMBOL (IC_RESULT (ic))->ismyparm && !OP_SYMBOL (IC_RESULT (ic))->islocal)
        crossedCall = 1;

      if (dic->op == CRITICAL)
        {
          dic = NULL;
          break;
        }

      if (SKIP_IC2 (dic))
        continue;

      if (dic->op == IFX)
        {
          if (IS_SYMOP (IC_COND (dic)) &&
              (IC_COND (dic)->key == IC_RESULT (ic)->key ||
               IC_COND (dic)->key == IC_RIGHT (ic)->key))
            {
              dic = NULL;
              break;
            }
        }
      else
        {
          if (IS_TRUE_SYMOP (IC_RESULT (dic)) && IS_OP_VOLATILE (IC_RESULT (dic)))
            {
              dic = NULL;
              break;
            }

          if (IS_SYMOP (IC_RESULT (dic)) && IC_RESULT (dic)->key == IC_RIGHT (ic)->key)
            {
              if (POINTER_SET (dic))
                dic = NULL;
              break;
            }

          if (IS_SYMOP (IC_RIGHT (dic)) &&
              (IC_RIGHT (dic)->key == IC_RESULT (ic)->key ||
               IC_RIGHT (dic)->key == IC_RIGHT (ic)->key))
            {
              dic = NULL;
              break;
            }

          if (IS_SYMOP (IC_LEFT (dic)) &&
              (IC_LEFT (dic)->key == IC_RESULT (ic)->key ||
               IC_LEFT (dic)->key == IC_RIGHT (ic)->key))
            {
              dic = NULL;
              break;
            }

          if (POINTER_SET (dic) && IC_RESULT (dic)->key == IC_RESULT (ic)->key)
            {
              dic = NULL;
              break;
            }

          if (crossedCall)
            {
              dic = NULL;
              break;
            }
        }
    }

  if (!dic)
    return 0;

  /* if assignment then check that right is not a bit */
  if (ASSIGNMENT (ic) && !POINTER_SET (ic))
    {
      sym_link *etype = operandType (IC_RESULT (dic));

      if (IS_BITFIELD (etype) && !IS_BITFIELD (operandType (IC_RESULT (ic))))
        return 0;
    }

  /* found the definition: move the live range, replace the result with
     this assignment's, and remove this assignment */
  for (sic = dic; sic != ic; sic = sic->next)
    {
      bitVectUnSetBit (sic->rlive, IC_RESULT (ic)->key);
      if (IS_ITEMP (IC_RESULT (dic)))
        bitVectSetBit (sic->rlive, IC_RESULT (dic)->key);
    }

  bitVectUnSetBit (OP_SYMBOL (IC_RESULT (dic))->defs, dic->key);
  ReplaceOpWithCheaperOp (&IC_RESULT (dic), IC_RESULT (ic));

  if (IS_ITEMP (IC_RESULT (dic)) && OP_SYMBOL (IC_RESULT (dic))->liveFrom > dic->seq)
    OP_SYMBOL (IC_RESULT (dic))->liveFrom = dic->seq;

  remiCodeFromeBBlock (ebp, ic);
  bitVectUnSetBit (OP_DEFS (IC_RESULT (ic)), ic->key);
  hTabDeleteItem (&iCodehTab, ic->key, ic, DELETE_ITEM, NULL);
  OP_DEFS (IC_RESULT (dic)) = bitVectSetBit (OP_DEFS (IC_RESULT (dic)), dic->key);
  return 1;
}

static void
packRegisters (ebbIndex *ebbi)
{
  int b;

  for (b = 0; b < ebbi->count; b++)
    {
      eBBlock *ebp = ebbi->bbOrder[b];
      int change;

      do
        {
          iCode *ic;

          change = 0;
          for (ic = ebp->sch; ic; ic = ic->next)
            if (ic->op == '=' && !POINTER_SET (ic))
              change += packRegsForAssign (ic, ebp);
        }
      while (change);
    }

  /* the packing moved live ranges */
  recomputeLiveRanges (ebbi->bbOrder, ebbi->count, false);
}

/*-----------------------------------------------------------------*/
/* markRemat - mark every iTemp whose only value is a link-time     */
/* constant address (&sym + k) as rematerializable                  */
/*                                                                   */
/* The same rules as hc08's packRegisters(): the result of taking a  */
/* static symbol's address, a constant added to or subtracted from    */
/* one, a pointer cast of one, and a plain copy of one -- each the    */
/* temp's only definition. gen.c gives such a temp AOP_IMMD instead   */
/* of a spill location, so `s[2]++` on a global array is a direct      */
/* load and store of s + 4, not an address computed into memory and     */
/* walked through X.                                                    */
/*-----------------------------------------------------------------*/
static void
markRemat (ebbIndex *ebbi)
{
  int b;

  for (b = 0; b < ebbi->count; b++)
    {
      iCode *ic;

      for (ic = ebbi->bbOrder[b]->sch; ic; ic = ic->next)
        {
          operand *res = IC_RESULT (ic);

          if (POINTER_SET (ic) || !res || !IS_ITEMP (res) ||
              bitVectnBitsOn (OP_DEFS (res)) != 1 ||
              OP_SYMBOL (res)->_isparm || OP_SYMBOL (res)->addrtaken)
            continue;

          if (ic->op == ADDRESS_OF &&
              IS_TRUE_SYMOP (IC_LEFT (ic)) && !OP_SYMBOL (IC_LEFT (ic))->onStack &&
              (!IC_RIGHT (ic) || IS_OP_LITERAL (IC_RIGHT (ic))))
            {
              OP_SYMBOL (res)->remat = 1;
              OP_SYMBOL (res)->rematiCode = ic;
            }
          else if ((ic->op == '+' || ic->op == '-') &&
                   IS_ITEMP (IC_LEFT (ic)) && OP_SYMBOL (IC_LEFT (ic))->remat &&
                   IS_OP_LITERAL (IC_RIGHT (ic)) && IS_PTR (operandType (res)))
            {
              OP_SYMBOL (res)->remat = 1;
              OP_SYMBOL (res)->rematiCode = ic;
            }
          else if ((ic->op == CAST || ic->op == '=') &&
                   IS_ITEMP (IC_RIGHT (ic)) && OP_SYMBOL (IC_RIGHT (ic))->remat &&
                   IS_PTR (operandType (res)) && getSize (operandType (res)) == 2)
            {
              OP_SYMBOL (res)->remat = 1;
              OP_SYMBOL (res)->rematiCode = ic;
            }
        }
    }
}

/*-----------------------------------------------------------------*/
/* lh5801_assignRegisters - phase-1 entry point: spill everything, */
/* then generate code                                              */
/*-----------------------------------------------------------------*/
void
lh5801_assignRegisters (ebbIndex *ebbi)
{
  symbol *sym;
  int k;
  value *arg;

  /* A reentrant or variadic function's own named parameters are on the
     real hardware stack, not at a fixed static address (SDCCmem.c's
     allocParms() -- HASVARARGS forces ISREENT, see SDCCsymt.c's
     processFunc()) -- give each one a static shadow location the exact
     same way an itemp spill gets one below, so genFunction()'s prologue
     (gen.c) has somewhere to copy the real, caller-pushed value into
     once, up front. Must run before genLH5801Code(): that's what
     actually reads psym->usl.spillLoc while emitting the copy. */
  for (arg = FUNC_ARGS (currFunc->type); arg; arg = arg->next)
    {
      symbol *psym = arg->sym;

      if (psym && psym->onStack && !psym->isspilt)
        createStaticSpil (psym);
    }

  packRegisters (ebbi);
  markRemat (ebbi);

  for (sym = hTabFirstItem (liveRanges, &k); sym; sym = hTabNextItem (liveRanges, &k))
    {
      if (!sym->isitmp)
        continue;
      if (sym->isspilt || sym->remat)
        continue;
      if (sym->liveFrom == sym->liveTo && !bitVectnBitsOn (sym->defs))
        continue;

      /* A void-typed itemp (confirmed via a void-returning PCALL used as
         a bare statement, e.g. calling a `void (*)(void) __reentrant`
         through a function pointer: SDCCicode.c still creates a "result"
         itemp for the call even though nothing ever reads it) has size 0
         and carries no actual data -- giving it a real spill location
         would emit a symbol SDCCglue.c then rejects with E_UNKNOWN_SIZE
         ("attempt to allocate variable of unknown size") when it tries
         to size its storage. It needs no storage at all, so just skip it. */
      if (getSize (sym->type) == 0)
        continue;

      createStaticSpil (sym);
    }

  genLH5801Code (iCodeFromeBBlock (ebbi->bbOrder, ebbi->count));

  /* See genLH5801OrphanedLabels()'s own comment (gen.c): a still-live
     branch can target a label whose own block iCodeFromeBBlock() just
     silently dropped as unreachable, leaving sdas with a dangling
     reference. This makes sure every such label still gets defined
     somewhere, even though the code there is genuinely dead. */
  genLH5801OrphanedLabels (ebbi->bbOrder, ebbi->count);

  slocNum = 0;
}
