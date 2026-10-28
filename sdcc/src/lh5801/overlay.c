/*-------------------------------------------------------------------------
  overlay.c - overlaying the LH5801 port's function-local storage.

  Paul Chambre, 2026

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  With no stack frames (stackAuto is off, see main.c), every local and
  every spilled iTemp is a static byte of its own -- though two functions
  that are never active at the same time could share theirs. SDCC's own
  overlay segment only does that for functions that call nothing; this
  lays a module's locals out as a "compiled stack" instead: each
  function's frame starts after the frames of every function in the
  module that can be active when it runs (the longest chain of callers
  above it in the module's call graph), so frames of functions that never
  nest share memory.

  What it knows, and what it assumes:
    - the direct calls each function makes (CALL iCodes), and whether it
      calls through a pointer, calls another module or has inline asm;
    - which functions have their address taken: SDCC's addrtaken, which
      only an explicit `&f` sets, plus every function named other than as
      a call's target (markFuncOperand(), and SDCCglue.c for initializers);
    - a call to another module, through a pointer, or inline asm may call
      back into any of this module's non-static or address-taken
      functions (the library calls putchar() by name) -- except main(),
      which nothing calls back;
    - an address-taken or reentrant function, and any function on a cycle
      (recursion, or a callback cycle the rule above makes), keeps storage
      of its own: frame size 0 in the layout, but still a link in its
      callers' chains;
    - parameters stay where they are: a caller fills a callee's _PARM_n
      bytes while it may still call something else first (`g (x, h (y))`),
      and h's frame starts where g's would. Statics stay too, of course.
      (Register parameters arrive in A or X; where the callee keeps them
      is an ordinary local.)

  lh5801_overlayFunction() runs at the end of each function's register
  allocation (ralloc.c): it takes the function's overlayable locals out of
  the data map, so SDCCglue.c doesn't emit them, and records them with the
  function's calls. lh5801_overlayEmit() runs at the end of the module
  (main.c's genAssemblerEnd): it lays the frames out and emits one DATA
  block with every local defined as an offset into it. The startup code
  zeroes DATA, so overlaid locals start at 0 like any others.
-------------------------------------------------------------------------*/

#include "common.h"
#include "ralloc.h"
#include "gen.h"

bool lh5801_noLocalOverlay = false;

typedef struct ovlLocal
{
  symbol *sym;
  int size;
  bool saved;               /* in the reentrant function's saved block */
} ovlLocal;

typedef struct ovlFunc
{
  symbol *func;
  ovlLocal *locals;
  int nLocals;
  int size;                 /* sum of the locals' sizes */
  char **callees;           /* rnames of the functions it calls directly */
  int nCallees;
  bool callsOut;            /* another module, a pointer, or inline asm */
  int start;                /* frame offset in the module's block */
  bool own;                 /* keeps storage of its own */
  bool saves;               /* has a saved block (gen.c's collectFrameSave()) */
} ovlFunc;

static ovlFunc *funcs;
static int nFuncs, capFuncs;

static bool
overlayable (const symbol *sym)
{
  return sym->level > 0 && sym->localof == currFunc && !sym->_isparm && !IS_STATIC (sym->etype) &&
         !IS_EXTERN (sym->etype) && !IS_FUNC (sym->type) && (sym->allocreq || IS_AGGREGATE (sym->type)) &&
         getSize (sym->type) > 0;
}

/* A function named other than as a call's target (`f (cb)`, `p = cb`)
   has its address taken -- which SDCC only records for an explicit `&`.
   (SDCCglue.c does the same for initializers, `fp t[] = { cb }`.) */
static void
markFuncOperand (operand *op)
{
  if (op && IS_SYMOP (op) && IS_FUNC (OP_SYMBOL (op)->type))
    OP_SYMBOL (op)->addrtaken = 1;
}

void
lh5801_overlayFunction (ebbIndex *ebbi)
{
  ovlFunc *f;
  symbol *sym;
  set *mine = NULL;
  int b, saved = 0;

  /* a saved block is laid out here even without the overlay */
  if (!currFunc || (lh5801_noLocalOverlay && !lh5801_frameSaveSize))
    return;

  if (nFuncs == capFuncs)
    {
      capFuncs = capFuncs ? 2 * capFuncs : 32;
      funcs = Safe_realloc (funcs, capFuncs * sizeof (*funcs));
    }
  f = &funcs[nFuncs++];
  memset (f, 0, sizeof (*f));
  f->func = currFunc;

  for (sym = setFirstItem (data->syms); sym; sym = setNextItem (data->syms))
    if (overlayable (sym))
      addSet (&mine, sym);
  for (sym = setFirstItem (mine); sym; sym = setNextItem (mine))
    {
      f->locals = Safe_realloc (f->locals, (f->nLocals + 1) * sizeof (*f->locals));
      f->locals[f->nLocals].sym = sym;
      f->locals[f->nLocals].size = getSize (sym->type);
      f->locals[f->nLocals].saved = isinSet (lh5801_frameSave, sym);
      if (f->locals[f->nLocals].saved)
        saved += f->locals[f->nLocals].size;
      f->size += f->locals[f->nLocals].size;
      f->nLocals++;
      deleteSetItem (&data->syms, sym);
    }
  deleteSet (&mine);
  wassertl (saved == lh5801_frameSaveSize, "lh5801: a saved frame slot is not one of the function's locals");
  f->saves = saved > 0;

  for (b = 0; b < ebbi->count; b++)
    {
      iCode *ic;

      for (ic = ebbi->bbOrder[b]->sch; ic; ic = ic->next)
        {
          if (ic->op == PCALL || ic->op == INLINEASM)
            f->callsOut = true;
          else if (ic->op == CALL && IS_SYMOP (IC_LEFT (ic)))
            {
              f->callees = Safe_realloc (f->callees, (f->nCallees + 1) * sizeof (char *));
              f->callees[f->nCallees++] = Safe_strdup (OP_SYMBOL (IC_LEFT (ic))->rname);
            }
          if (!SKIP_IC2 (ic))
            {
              /* a CALL's and a RECEIVE's (its own function) don't count */
              if (ic->op != CALL && ic->op != RECEIVE)
                markFuncOperand (IC_LEFT (ic));
              markFuncOperand (IC_RIGHT (ic));
              markFuncOperand (IC_RESULT (ic));
            }
        }
    }
}

static int
funcIndex (const char *rname)
{
  int i;

  for (i = 0; i < nFuncs; i++)
    if (!strcmp (funcs[i].func->rname, rname))
      return i;
  return -1;
}

/* Can a call out of the module come back to this function? */
static bool
callableBack (const ovlFunc *g)
{
  return (!IS_STATIC (g->func->etype) || g->func->addrtaken) && strcmp (g->func->name, "main");
}

void
lh5801_overlayEmit (FILE *of)
{
  bool *edge;
  int i, j, k, pass, total = 0;

  if (!nFuncs)
    return;

  /* the call graph, f -> g: f can be active while g runs */
  edge = Safe_alloc (nFuncs * nFuncs * sizeof (bool));
  for (i = 0; i < nFuncs; i++)
    {
      ovlFunc *f = &funcs[i];

      for (k = 0; k < f->nCallees; k++)
        {
          j = funcIndex (f->callees[k]);
          if (j >= 0)
            edge[i * nFuncs + j] = true;
          else
            f->callsOut = true;
        }
      if (f->callsOut)
        for (j = 0; j < nFuncs; j++)
          if (callableBack (&funcs[j]))
            edge[i * nFuncs + j] = true;
    }

  /* functions with storage of their own: address-taken, reentrant, or on
     a cycle (reach[i][i], by transitive closure) */
  {
    bool *reach = Safe_alloc (nFuncs * nFuncs * sizeof (bool));

    memcpy (reach, edge, nFuncs * nFuncs * sizeof (bool));
    for (k = 0; k < nFuncs; k++)
      for (i = 0; i < nFuncs; i++)
        if (reach[i * nFuncs + k])
          for (j = 0; j < nFuncs; j++)
            if (reach[k * nFuncs + j])
              reach[i * nFuncs + j] = true;
    for (i = 0; i < nFuncs; i++)
      funcs[i].own = funcs[i].func->addrtaken || IFFUNC_ISREENT (funcs[i].func->type) ||
                     reach[i * nFuncs + i];
    Safe_free (reach);
  }

  /* longest caller chain above each frame -- frames of their own count 0,
     so no cycle can grow it, and nFuncs passes always settle */
  for (pass = 0; pass < nFuncs; pass++)
    for (i = 0; i < nFuncs; i++)
      {
        int end = funcs[i].start + (funcs[i].own ? 0 : funcs[i].size);

        for (j = 0; j < nFuncs; j++)
          if (edge[i * nFuncs + j] && funcs[j].start < end)
            funcs[j].start = end;
      }
  for (i = 0; i < nFuncs; i++)
    if (!funcs[i].own && funcs[i].start + funcs[i].size > total)
      total = funcs[i].start + funcs[i].size;

  fprintf (of, ";--------------------------------------------------------\n");
  fprintf (of, "; overlaid function locals (src/lh5801/overlay.c)\n");
  fprintf (of, ";--------------------------------------------------------\n");
  fprintf (of, "\t.area DATA\n");
  if (total)
    {
      fprintf (of, "__lh5801_overlay:\n");
      fprintf (of, "\t.ds %d\n", total);
    }
  for (i = 0; i < nFuncs; i++)
    {
      ovlFunc *f = &funcs[i];
      int off = f->start;

      if (f->nLocals)
        fprintf (of, "; %s: %d bytes%s\n", f->func->name, f->size,
                 f->own ? ", its own" : "");
      /* the saved block first, in one piece */
      if (f->saves)
        {
          fprintf (of, "%s__saved:\n", f->func->rname);
          for (k = 0; k < f->nLocals; k++)
            if (f->locals[k].saved)
              fprintf (of, "%s:\n\t.ds %d\n", f->locals[k].sym->rname, f->locals[k].size);
        }
      for (k = 0; k < f->nLocals; k++)
        {
          if (f->locals[k].saved)
            continue;
          if (f->own)
            fprintf (of, "%s:\n\t.ds %d\n", f->locals[k].sym->rname, f->locals[k].size);
          else
            {
              fprintf (of, "%s = __lh5801_overlay + %d\n", f->locals[k].sym->rname, off);
              off += f->locals[k].size;
            }
        }
    }
  fprintf (of, "\t.area CODE\n");

  Safe_free (edge);
  for (i = 0; i < nFuncs; i++)
    {
      for (k = 0; k < funcs[i].nCallees; k++)
        Safe_free (funcs[i].callees[k]);
      Safe_free (funcs[i].callees);
      Safe_free (funcs[i].locals);
    }
  Safe_free (funcs);
  funcs = NULL;
  nFuncs = capFuncs = 0;
}
