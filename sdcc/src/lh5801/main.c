/*-------------------------------------------------------------------------
  main.c - LH5801 specific definitions.

  Paul Chambre, 2026

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.
-------------------------------------------------------------------------*/

#include "common.h"

#include "ralloc.h"
#include "gen.h"

/*-----------------------------------------------------------------*/
/* lh5801_init - one-time port init: hook up the sdas/aslh5801     */
/* ("asxxxx"-family) assembly-text conventions (!tlabel, !area,    */
/* etc. -- see SDCCasm.c's _asxxxx_mapping)                        */
/*-----------------------------------------------------------------*/
static void
lh5801_init (void)
{
  asm_addTree (&asm_asxxxx_mapping);
}

static void
lh5801_setDefaultOptions (void)
{
  /* Phase 1: no register allocation, and no real stack frames either --
     locals and iTemps all land in static "data" storage via the same
     allocLocal() path true globals use (see SDCCmem.c: with stackAuto=0,
     S_AUTO symbols route to port->mem.default_local_map instead of a
     stack frame). See ralloc.c/ralloc.h for why this is a deliberate,
     documented phase-1 simplification, not an oversight. */
  options.stackAuto = 0;
  options.noRegParams = 1;

  /* Without this, a `register`-qualified local (confirmed via
     `register unsigned char c = ...` in device/lib/printf_large.c, which
     printf's own internal helpers use) routes through allocLocal()'s
     S_REGISTER branch (SDCCmem.c) to the generic `overlay` memory
     segment -- a call-tree-based memory-reuse scheme for ports with a
     real register allocator to decide when it's safe, which we never
     configured (nor would benefit from: phase 1 spills everything
     unconditionally, so there's nothing to "instead of register"
     overlay). The result was a NULL output storage class and a FATAL
     "Failed to allocate symbol to memory segment" ICE the moment any
     source used `register` at all. This forces S_REGISTER through the
     exact same plain "data" segment path as every other spilt local. */
  options.noOverlay = 1;

  /* Both arbitrary addresses within the bare PC-1500's built-in 2K RAM
     (4000H-47FFH), clear of the 4000H-40C4H reserve area that's live
     firmware state, not free RAM -- see pc1500emu's
     docs/pc1500_hardware_reference.md. Deliberately NOT the C000H+ ROM
     region: phase 1 only ever links stand-alone test binaries loaded
     directly into RAM (pc1500emu's "loadbinary" FIFO command), not real
     ROM-resident firmware, so there's no reason to fight the emulator's
     ROM-write guard for now. */
  options.data_loc = 0x4600;
  options.code_loc = 0x4200;
  options.stack_loc = -1;    /* set by genInitStartup unconditionally, see below */

  options.out_fmt = 'i';     /* ihx */
}

static bool
lh5801_parseOptions (int *pargc, char **argv, int *i)
{
  (void) pargc;
  (void) argv;
  (void) i;
  return false; /* No port-specific options yet; port->parseOption is called
                   unconditionally from SDCCmain.c's parseCmdLine with no
                   NULL guard, unlike several other PORT hooks. */
}

static void
lh5801_finaliseOptions (void)
{
  port->mem.default_local_map = data;
  port->mem.default_globl_map = data;
}

static const char *
lh5801_getRegName (const struct reg_info *reg)
{
  if (reg)
    return reg->name;
  return "err";
}

static void
lh5801_reset_regparms (sym_link *ftype)
{
  (void) ftype;
}

static int
lh5801_reg_parm (sym_link *ftype, bool reentrant)
{
  (void) ftype;
  (void) reentrant;
  return 0; /* Phase 1 never passes parameters in registers. */
}

static bool
lh5801_hasNativeMulFor (iCode *ic, sym_link *left, sym_link *right)
{
  (void) ic;
  (void) left;
  (void) right;
  return false; /* LH5801 has no multiply instruction at all. */
}

static bool
lh5801_hasExtBitOp (int op, sym_link *left, int right)
{
  (void) op;
  (void) left;
  (void) right;
  return false; /* Not implemented in the phase-1 backend. */
}

/*-----------------------------------------------------------------*/
/* lh5801_genIVT - required by SDCCglue.c's createInterruptVect()  */
/* (called whenever glue_up_main is set, which we need for other   */
/* reasons -- see the PORT struct comment above). No real vector   */
/* table format is defined yet (interrupts aren't implemented in   */
/* the phase-1 backend); just satisfy the "did the port handle      */
/* this" check with an empty table.                                */
/*-----------------------------------------------------------------*/
static int
lh5801_genIVT (struct dbuf_s *oBuf, symbol **intTable, int intCount)
{
  (void) oBuf;
  (void) intTable;
  (void) intCount;
  return TRUE;
}

/*-----------------------------------------------------------------*/
/* lh5801_genInitStartup - startup code: set S, call main, then    */
/* loop forever (there is nothing sensible to "return" to on bare  */
/* hardware). Modeled on STM8's inline (no separate crt0.s)        */
/* approach -- stm8_genInitStartup in src/stm8/main.c.              */
/*                                                                  */
/* Phase 1 gap, deliberate: does NOT zero the DATA segment or copy  */
/* INITIALIZER into INITIALIZED. Fine for a smoke test that never   */
/* relies on implicit zero-initialization; revisit once codegen    */
/* correctness beyond '=' is established.                          */
/*-----------------------------------------------------------------*/
static void
lh5801_genInitStartup (FILE *of)
{
  fprintf (of, "\tldi\ts,0x47ff\n");
  fprintf (of, "\tsjp\t_main\n");
  fprintf (of, "__sdcc_lh5801_forever:\n");
  fprintf (of, "\tbch\t__sdcc_lh5801_forever\n");
}

/* isTargetKeyword() (SDCCglue.c) returns false for every __-prefixed
   keyword when this array is empty -- including "reentrant" -- so the
   lexer silently falls back to treating "__reentrant" as a plain
   identifier (SDCC.lex's TKEYWORD macro) instead of the REENTRANT token,
   producing a generic syntax error wherever it's used. Needed for real
   function-pointer calls with arguments to work at all: SDCC's frontend
   requires a function type be reentrant before allowing it to be called
   indirectly with any arguments (see the E_NONRENT_ARGS check in
   SDCCsymt.c) -- e.g. printf's own internal pfn_outputchar callback
   (device/lib/printf_large.c) needs this. Note this is independent of
   the _REENTRANT *macro* being defined empty for this port in
   asm/lh5801/features.h -- that's a separate, still-necessary workaround
   for a real SDCC.y grammar gap (REENTRANT isn't a valid production on a
   typedef'd declarator, only on direct function declarations/
   definitions); this array only affects the literal __reentrant/
   reentrant keyword being recognized when C source uses it directly. */
static char *lh5801_keywords[] = {
  "reentrant",
  NULL
};

static const char *_asmCmd[] =
{
  "sdaslh5801", "$l", "$3", "\"$1.asm\"", NULL
};

/* sdld is a single generic, port-agnostic linker shared by every sdas
   target (see sdas/linksrc/) -- there is no dedicated "sdldlh5801"
   binary, matching how mcs51 references plain "sdld" directly rather
   than a per-port copy. */
static const char *_linkCmd[] =
{
  "sdld", "-nf", "\"$1\"", "$L", NULL
};

PORT lh5801_port =
{
  TARGET_ID_LH5801,
  "lh5801",
  "LH5801",                     /* Target name */
  NULL,                         /* Processor name */
  {
    glue,
    TRUE,                       /* glue_up_main: MUST be TRUE -- SDCCglue.c's
                                    global `mainf` (whether main() was found
                                    and has a body) is *only* ever set as a
                                    side effect of createInterruptVect(),
                                    which itself only runs when glue_up_main
                                    is set. Every other main-related check in
                                    glue() (including the one gating our own
                                    genInitStartup!) reads that same global,
                                    so leaving this FALSE silently skips
                                    genInitStartup entirely -- confirmed by
                                    getting an empty .asm with no startup code
                                    or main() body at all. This in turn
                                    requires a real genIVT() (below) to
                                    satisfy createInterruptVect()'s assert,
                                    and TARGET_IS_LH5801 branches added to
                                    SDCCglue.c's two hardcoded mcs51-style
                                    "__sdcc_program_startup" mnemonic choices
                                    (dead code for us -- our genInitStartup
                                    jumps straight to _main itself -- but it
                                    still has to *assemble*). */
    0,                          /* no model variants (yet) */
    0,
    NULL,
  },
  {                             /* Assembler */
    _asmCmd,
    NULL,
    "-l",                       /* Options with debug */
    "-l",                       /* Options without debug */
    0,
    ".asm",
    NULL,                       /* do_assemble */
  },
  {                             /* Linker */
    _linkCmd,
    NULL,
    NULL,
    ".rel",
    1,
    NULL,                       /* crt */
    NULL,                       /* libs -- none yet */
  },
  {                             /* Peephole optimizer -- no rules yet */
    "",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
  },
  /* Sizes -- a flat 16-bit address space, same numbers as STM8 (also a
     big-endian, no-banking 8-bit target with a 16-bit address space). */
  {
    1,                          /* char */
    2,                          /* short */
    2,                          /* int */
    4,                          /* long */
    8,                          /* long long */
    2,                          /* near ptr */
    2,                          /* far ptr */
    2,                          /* generic ptr */
    2,                          /* func ptr */
    2,                          /* banked func ptr -- no banking */
    1,                          /* bit */
    4,                          /* float */
    64,                         /* bit-precise integer types up to _BitInt (64) */
  },
  /* tags for generic pointers -- no distinction needed, flat address space */
  { 0x00, 0x00, 0x00, 0x00 },
  {
    NULL,                       /* xstack -- no separate extended stack */
    "STACK",                    /* istack */
    "CODE",                     /* code */
    "DATA",                     /* data */
    NULL,                       /* idata */
    NULL,                       /* pdata */
    NULL,                       /* xdata */
    NULL,                       /* xconst_name */
    NULL,                       /* bit */
    NULL,                       /* reg -- no register-bank concept */
    "GSINIT",                   /* static initialization */
    NULL,                       /* overlay */
    "GSFINAL",                  /* gsfinal */
    "HOME",                     /* home */
    NULL,                       /* xidata */
    NULL,                       /* xinit */
    "CONST",                    /* const_name */
    "CABS (ABS)",               /* cabs_name */
    "DABS (ABS)",               /* xabs_name */
    NULL,                       /* iabs_name */
    "INITIALIZED",              /* name of segment for initialized globals/statics */
    "INITIALIZER",              /* code copy of the above, for startup to copy from */
    NULL,                       /* default_local_map -- set in finaliseOptions */
    NULL,                       /* default_globl_map -- set in finaliseOptions */
    1,                          /* CODE is read-only */
    false,                      /* no __sfr on this port */
    1                           /* no fancy alignments supported */
  },
  { NULL, NULL },               /* extraAreas */
  1,                            /* default ABI revision */
  {                             /* stack information */
    -1,                         /* grows down */
     0,                         /* bank_overhead -- no banking */
     0,                         /* isr_overhead -- ISRs not implemented yet */
     2,                         /* call_overhead: SJP pushes a 2-byte return address */
     0,                         /* reent_overhead */
     0,                         /* banked_overhead -- no banking */
     1,                         /* offset: our S register always points at
                                    the next FREE byte, never the last one
                                    pushed (push8() in pc1500emu's CPU core
                                    writes at the current S, then
                                    decrements) -- confirmed empirically
                                    against a real compiled reentrant call.
                                    Not read by any shared SDCC code (grep
                                    confirms the only consumer is a verbose
                                    debug-dump function in SDCCmem.c) -- this
                                    port's own gen.c hardcodes the same "+1"
                                    directly rather than reading this field,
                                    so it's documentation only. */
  },
  {
    -1,                         /* shift: not implemented yet, but claim
                                    arbitrary-size support so the optimizer
                                    doesn't try to call a nonexistent
                                    support routine for it */
    false,                      /* has_mulint2long -- no multiply support yet */
    false,                      /* has_mululonguchar2ulonglong */
  },
  { NULL,                       /* emitDebuggerSymbol */
    {
      NULL,                     /* regNum */
      0,                        /* cfiSame */
      0,                        /* cfiUndef */
      2,                        /* addressSize */
      0,                        /* regNumRet */
      SP_IDX,                   /* regNumSP */
      0,                        /* regNumBP */
      0,                        /* offsetSP */
    },
  },
  {
    0,                          /* maxCount: 0 disables jump-table codegen
                                    for switch(), which isn't implemented yet */
    2,
    {0, 0, 0},
    {0, 0, 0},
    0,
    0,
  },
  "_",
  lh5801_init,
  lh5801_parseOptions,
  NULL,                         /* poptions */
  NULL,                         /* initPaths */
  lh5801_finaliseOptions,
  lh5801_setDefaultOptions,
  lh5801_assignRegisters,
  lh5801_getRegName,
  0,                            /* getRegByName */
  NULL,                         /* rtrackUpdate */
  lh5801_keywords,
  genLH5801AssemblerStart,
  NULL,                         /* genAssemblerEnd */
  lh5801_genIVT,
  0,                            /* genXINIT -- inlined into genInitStartup instead */
  lh5801_genInitStartup,
  lh5801_reset_regparms,
  lh5801_reg_parm,
  NULL,                         /* process_pragma */
  NULL,                         /* getMangledFunctionName */
  lh5801_hasNativeMulFor,
  lh5801_hasExtBitOp,
  NULL,                         /* oclsExpense */
  TRUE,                         /* use_dw_for_init */
  false,                        /* little_endian -- LH5801 is big endian
                                    (confirmed via SJP/STA absolute operand
                                    byte order in the sdas/aslh5801 linker
                                    test) */
  0,                            /* leave lt */
  0,                            /* leave gt */
  1,                            /* transform <= to ! > */
  1,                            /* transform >= to ! < */
  false,                        /* leave != */
  false,                        /* leave == */
  false,                        /* Array initializer support -- not yet */
  NULL,                         /* cseOk */
  "",                           /* c_preamble */
  GPOINTER,                     /* unqualified pointers are "generic" */
  false,                        /* no __far */
  false,                        /* no __far */
  /* MUST be 0, not 1: resetting labelKey per function only works if the
     *assembler* resolves "NNNNN$"-style numbered local labels (SDCCasm.c's
     "!tlabel"/"!tlabeldef" -- labelKey2num() is just key+100, no per-
     function salt at all) to whichever same-numbered definition is
     nearest the referencing branch, not to a single, file-wide symbol.
     aslh5801 does no such thing -- confirmed directly: no "$" handling
     anywhere in aslex.c or assym.c (the shared asxxxx-family lexer/
     symbol-table code aslh5801 is built from), so it treats "00103$" as
     one ordinary, file-global symbol name like any other. With
     reset_labelKey=1, two *different*, unrelated functions can and did
     each define their own "00103$" (confirmed directly in
     device/lib/printf_large.c's compiled output -- two real, distinct
     "00103$:" definitions in one file), and a branch landing between them
     resolves against whichever one the assembler's ordinary global
     symbol lookup happens to find, not the intended nearest one --
     producing exactly the "undefined symbol"/"branches must target a
     symbol in the same area" assembler errors that blocked
     printf_large.c from assembling. Setting this to 0 keeps labelKey
     monotonically increasing across the whole compilation instead, so
     every rendered label is genuinely, globally unique -- correct for an
     assembler with no local-label scoping at all. */
  0,                            /* do NOT reset labelKey per function */
  1,                            /* globals & local statics allowed */
  2,                            /* num_regs -- unused (no tree-decomposition
                                    allocator in phase 1), matches lh5801_regs[] */
  PORT_MAGIC
};
