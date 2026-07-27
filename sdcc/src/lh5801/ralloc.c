/*-------------------------------------------------------------------------
  ralloc.c - register allocation for the LH5801 port.

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

  allocLocal (sloc);
  sloc->isref = 1;

  sym->usl.spillLoc = sloc;
  sym->isspilt = 1;
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

  for (sym = hTabFirstItem (liveRanges, &k); sym; sym = hTabNextItem (liveRanges, &k))
    {
      if (!sym->isitmp)
        continue;
      if (sym->isspilt || sym->remat)
        continue;
      if (sym->liveFrom == sym->liveTo && !bitVectnBitsOn (sym->defs))
        continue;

      createStaticSpil (sym);
    }

  genLH5801Code (iCodeFromeBBlock (ebbi->bbOrder, ebbi->count));

  slocNum = 0;
}
