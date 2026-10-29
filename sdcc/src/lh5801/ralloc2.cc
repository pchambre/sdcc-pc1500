// ralloc2.cc - the LH5801 port's glue to SDCC's tree-decomposition register
// allocator (SDCCralloc.hpp, by Philipp Klaus Krause), modeled on
// src/hc08/ralloc2.cc.
//
// Paul Chambre, 2026
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option) any
// later version.
//
// The allocator decides, for every byte of every iTemp ralloc.c marked
// for_newralloc, which register holds it at each point (or none), and
// finds the assignment of least total cost. The cost of an instruction
// under a candidate assignment is the size in bytes of the code gen.c
// would emit for it (drylh5801iCode(): the real code generator, with
// output switched off) -- so whatever gen.c can do with a register, the
// allocator weighs exactly.
//
// What gen.c can safely do with registers so far is what inst_sane()
// allows:
//   - only values of one or two bytes live in registers, wholly;
//   - only in the registers lh5801_reg_mask allows (main.c, --lh5801-regs),
//     and never A: it's gen.c's scratch inside every iCode;
//   - nothing in a register survives a call or inline asm (they preserve
//     no registers);
//   - at the few iCodes where gen.c uses X itself -- pointer accesses,
//     returns, a stack parameter's address -- nothing is in X but the
//     operand that is X, whole (x_sane()).
// In this port's generated code nothing else touches Y or U, so a byte in
// YL..UH is safe everywhere but across a call.

#define TD_SALLOC
#define CH_SALLOC

#include "SDCCralloc.hpp"

extern "C"
{
  #include "ralloc.h"
  #include "gen.h"
}

// The registers gen.c can't keep a live value in yet (see above).
static const unsigned int unsafe_regs = 1u << LH5801_REG_A;

// A result byte may share a register with an operand byte only when it's
// the same byte of a same-size operand: every gen*() reads byte i of its
// operands before it writes byte i of its result, but in no fixed order
// across bytes (add/sub/compare go low byte first, copies and bitwise ops
// high byte first, genWiden() shifts byte positions) -- so any other
// sharing could overwrite an operand byte that's still to be read.
template <class I_t>
static void add_operand_conflicts_in_node(const cfg_node &n, I_t &I)
{
  const iCode *ic = n.ic;
  const operand *result = IC_RESULT(ic);

  if (!result || !IS_SYMOP(result) || POINTER_SET(ic))
    return;

  operand_map_t::const_iterator oir, oir_end, oirs;
  boost::tie(oir, oir_end) = n.operands.equal_range(OP_SYMBOL_CONST(result)->key);
  if (oir == oir_end)
    return;

  const operand *ops[2] = {IC_LEFT(ic), IC_RIGHT(ic)};
  for (int k = 0; k < 2; k++)
    {
      const operand *o = ops[k];
      if (!o || !IS_SYMOP(o) || OP_SYMBOL_CONST(o)->key == OP_SYMBOL_CONST(result)->key)
        continue;
      operand_map_t::const_iterator oio, oio_end;
      for (boost::tie(oio, oio_end) = n.operands.equal_range(OP_SYMBOL_CONST(o)->key); oio != oio_end; ++oio)
        for (oirs = oir; oirs != oir_end; ++oirs)
          {
            var_t rvar = oirs->second;
            var_t ovar = oio->second;
            if (I[rvar].size != I[ovar].size || I[rvar].byte != I[ovar].byte)
              boost::add_edge(rvar, ovar, I);
          }
    }
}

// The register byte `v` of the assignment is in, or -1.
static reg_t var_reg(const assignment &a, var_t v)
{
  return std::binary_search(a.local.begin(), a.local.end(), v) ? a.global[v] : -1;
}

// An operand is either wholly in registers or wholly in memory, and at
// most two bytes wide if in registers (pointers and ints; any two
// registers -- gen.c reaches each byte on its own).
template <class G_t, class I_t>
static bool operand_sane(const operand *o, const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  if (!o || !IS_SYMOP(o))
    return true;

  operand_map_t::const_iterator oi, oi_end;
  boost::tie(oi, oi_end) = G[i].operands.equal_range(OP_SYMBOL_CONST(o)->key);
  if (oi == oi_end)
    return true;

  bool inreg = var_reg(a, oi->second) >= 0;
  if (inreg && I[oi->second].size > 2)
    return false;
  for (++oi; oi != oi_end; ++oi)
    if ((var_reg(a, oi->second) >= 0) != inreg)
      return false;
  return true;
}

static bool in_x(reg_t r)
{
  return r == LH5801_REG_XL || r == LH5801_REG_XH;
}

// Operand o's registers: regs[b] for byte b (0 = high), -1 in memory.
template <class G_t, class I_t>
static int operand_regs(const operand *o, const assignment &a, unsigned short int i, const G_t &G, const I_t &I, reg_t regs[2])
{
  regs[0] = regs[1] = -1;
  if (!o || !IS_SYMOP(o))
    return 0;
  int size = 0;
  operand_map_t::const_iterator oi, oi_end;
  for (boost::tie(oi, oi_end) = G[i].operands.equal_range(OP_SYMBOL_CONST(o)->key); oi != oi_end; ++oi)
    {
      size = I[oi->second].size;
      if (I[oi->second].byte < 2)
        regs[I[oi->second].byte] = var_reg(a, oi->second);
    }
  return size;
}

template <class G_t, class I_t>
static bool uses_x(const operand *o, const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  reg_t r[2];
  operand_regs(o, a, i, G, I, r);
  return in_x(r[0]) || in_x(r[1]);
}

// In X as a whole pointer: high byte in XH, low byte in XL -- the shape
// gen.c can use (or hand back from a call) as X itself.
template <class G_t, class I_t>
static bool exactly_x(const operand *o, const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  reg_t r[2];
  return operand_regs(o, a, i, G, I, r) == 2 && r[0] == LH5801_REG_XH && r[1] == LH5801_REG_XL;
}

// Is operand o's value dead after node i?
template <class G_t, class I_t>
static bool operand_dying(const operand *o, unsigned short int i, const G_t &G, const I_t &I)
{
  operand_map_t::const_iterator oi, oi_end;
  for (boost::tie(oi, oi_end) = G[i].operands.equal_range(OP_SYMBOL_CONST(o)->key); oi != oi_end; ++oi)
    if (G[i].dying.find(oi->second) == G[i].dying.end())
      return false;
  return true;
}

// X is gen.c's own register for a few iCodes: pointer accesses (it loads
// the pointer into X -- unless the pointer already is X), pushing a struct
// argument, the address of a stack parameter (loadStackAddrToX()), and
// calls and returns (a 2-byte value comes back in X; a call's stack
// arguments are popped through X).
// At those iCodes nothing may be in X but the operand that is X, whole;
// everywhere else gen.c doesn't touch X.
template <class G_t, class I_t>
static bool x_sane(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  const iCode *ic = G[i].ic;
  const operand *allowed = 0;       // the one operand that may be in X
  bool allowed_ok = true;           // ...if it is, is its shape usable?

  switch (ic->op)
    {
    case GET_VALUE_AT_ADDRESS:
      if (IS_ITEMP(IC_LEFT(ic)) && OP_SYMBOL_CONST(IC_LEFT(ic))->remat)
        return true;                // a constant address: direct, no X
      if (uses_x(IC_RESULT(ic), a, i, G, I))
        return false;
      allowed = IC_LEFT(ic);
      allowed_ok = exactly_x(allowed, a, i, G, I);
      // with an offset gen.c moves X to the element (loadPtrX()), so the
      // pointer can only be X if nothing reads it afterwards
      if (IS_SYMOP(allowed) && IC_RIGHT(ic) && IS_OP_LITERAL(IC_RIGHT(ic)) && operandLitValue(IC_RIGHT(ic)) &&
          !operand_dying(allowed, i, G, I))
        allowed_ok = false;
      break;
    case '=':
      if (!POINTER_SET(ic))
        return true;
      if (IS_ITEMP(IC_RESULT(ic)) && OP_SYMBOL_CONST(IC_RESULT(ic))->remat)
        return true;
      if (uses_x(IC_RIGHT(ic), a, i, G, I))
        return false;
      allowed = IC_RESULT(ic);      // the pointer
      allowed_ok = exactly_x(allowed, a, i, G, I);
      break;
    case IPUSH_VALUE_AT_ADDRESS:
      if (IS_ITEMP(IC_LEFT(ic)) && OP_SYMBOL_CONST(IC_LEFT(ic))->remat)
        return true;                // a constant address: direct, no X
      allowed = IC_LEFT(ic);        // X walks the struct, so it's spent
      allowed_ok = exactly_x(allowed, a, i, G, I) && operand_dying(allowed, i, G, I);
      break;
    case ADDRESS_OF:
      if (!IS_SYMOP(IC_LEFT(ic)) || !OP_SYMBOL_CONST(IC_LEFT(ic))->onStack)
        return true;
      allowed = IC_RESULT(ic);
      allowed_ok = exactly_x(allowed, a, i, G, I);
      break;
    case CALL:
    case PCALL:
      {
        reg_t r[2];
        int size = operand_regs(IC_RESULT(ic), a, i, G, I, r);
        allowed = IC_RESULT(ic);
        allowed_ok = ic->parmBytes < 2 && (size == 1 || exactly_x(allowed, a, i, G, I));
        if (ic->op == PCALL && uses_x(IC_LEFT(ic), a, i, G, I))
          return false;
      }
      break;
    case RETURN:
      {
        reg_t r[2];
        allowed = IC_LEFT(ic);
        allowed_ok = operand_regs(allowed, a, i, G, I, r) == 1 || exactly_x(allowed, a, i, G, I);
      }
      break;
    default:
      return true;
    }

  if (allowed && uses_x(allowed, a, i, G, I) && !allowed_ok)
    return false;
  for (cfg_alive_t::const_iterator v = G[i].alive.begin(); v != G[i].alive.end(); ++v)
    if (in_x(var_reg(a, *v)) && !(allowed && IS_SYMOP(allowed) && OP_SYMBOL_CONST(allowed)->key == I[*v].v))
      return false;
  return true;
}

template <class G_t, class I_t>
static bool inst_sane(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  const iCode *ic = G[i].ic;

  if (!operand_sane(IC_RESULT(ic), a, i, G, I) || !operand_sane(IC_LEFT(ic), a, i, G, I) ||
      !operand_sane(IC_RIGHT(ic), a, i, G, I))
    return false;

  if (!x_sane(a, i, G, I))
    return false;

  // Register parameters (gen.c's loadSends()/genReceive()): a 2-byte one
  // moves between X and its place through A a byte at a time, so a value
  // partly in X must be X exactly; and the 1-byte one arriving in A is
  // received first, so it can't go in X while the X one still waits there.
  if (ic->op == SEND && ic->argreg == 2 && uses_x(IC_LEFT(ic), a, i, G, I) && !exactly_x(IC_LEFT(ic), a, i, G, I))
    return false;
  if (ic->op == RECEIVE && ic->argreg == 2 && uses_x(IC_RESULT(ic), a, i, G, I) &&
      !exactly_x(IC_RESULT(ic), a, i, G, I))
    return false;
  if (ic->op == RECEIVE && ic->argreg == 1 && uses_x(IC_RESULT(ic), a, i, G, I))
    {
      for (const iCode *r = ic->prev; r && r->op == RECEIVE; r = r->prev)
        if (r->argreg == 2)
          return false;
      for (const iCode *r = ic->next; r && r->op == RECEIVE; r = r->next)
        if (r->argreg == 2)
          return false;
    }

  bool clobbers_all = ic->op == CALL || ic->op == PCALL || ic->op == INLINEASM;
  // ...except the Y and U halves a direct call's function is known to
  // leave alone (gen.c's recordPreservedRegs(), or __preserves_regs)
  sym_link *ftype = ic->op == CALL && IS_SYMOP(IC_LEFT(ic)) ? OP_SYMBOL(IC_LEFT(ic))->type : 0;

  for (cfg_alive_t::const_iterator v = G[i].alive.begin(); v != G[i].alive.end(); ++v)
    {
      reg_t r = var_reg(a, *v);
      if (r < 0)
        continue;
      if (!((lh5801_reg_mask & ~unsafe_regs) & (1u << r)))
        return false;
      if (clobbers_all && G[i].dying.find(*v) == G[i].dying.end() &&
          !(IC_RESULT(ic) && IS_SYMOP(IC_RESULT(ic)) && OP_SYMBOL_CONST(IC_RESULT(ic))->key == I[*v].v) &&
          !(ftype && IS_FUNC(ftype) && r >= LH5801_REG_YL && ftype->funcAttrs.preserved_regs[r]))
        return false;
    }
  return true;
}

template <class G_t, class I_t>
static void set_surviving_regs(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  iCode *ic = G[i].ic;

  bitVectClear(ic->rMask);
  bitVectClear(ic->rSurv);

  for (cfg_alive_t::const_iterator v = G[i].alive.begin(); v != G[i].alive.end(); ++v)
    {
      if (a.global[*v] < 0)
        continue;
      ic->rMask = bitVectSetBit(ic->rMask, a.global[*v]);

      if (G[i].dying.find(*v) == G[i].dying.end())
        if (!((IC_RESULT(ic) && !POINTER_SET(ic)) && IS_SYMOP(IC_RESULT(ic)) && OP_SYMBOL_CONST(IC_RESULT(ic))->key == I[*v].v))
          ic->rSurv = bitVectSetBit(ic->rSurv, a.global[*v]);
    }
}

template <class G_t, class I_t>
static void assign_operand_for_cost(operand *o, const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  if (!o || !IS_SYMOP(o))
    return;
  symbol *sym = OP_SYMBOL(o);
  operand_map_t::const_iterator oi, oi_end;
  for (boost::tie(oi, oi_end) = G[i].operands.equal_range(OP_SYMBOL_CONST(o)->key); oi != oi_end; ++oi)
    {
      var_t v = oi->second;
      if (a.global[v] >= 0)
        {
          sym->regs[I[v].byte] = lh5801_regs + a.global[v];
          sym->isspilt = false;
          sym->nRegs = I[v].size;
        }
      else
        {
          sym->isspilt = true;
          sym->nRegs = I[v].size;
          for (int b = 0; b < I[v].size; b++)
            sym->regs[b] = 0;
        }
    }
}

template <class G_t, class I_t>
static void assign_operands_for_cost(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  const iCode *ic = G[i].ic;

  assign_operand_for_cost(IC_LEFT(ic), a, i, G, I);
  assign_operand_for_cost(IC_RIGHT(ic), a, i, G, I);
  assign_operand_for_cost(IC_RESULT(ic), a, i, G, I);
}

template <class G_t, class I_t>
static float instruction_cost(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  iCode *ic = G[i].ic;
  float c;

  wassert(ic);

  if (!inst_sane(a, i, G, I))
    return std::numeric_limits<float>::infinity();

  if (ic->generated)
    return 0.0f;

  switch (ic->op)
    {
    // Register assignment doesn't matter for these:
    case FUNCTION:
    case ENDFUNCTION:
    case LABEL:
    case GOTO:
    case INLINEASM:
      return 0.0f;
    default:
      assign_operands_for_cost(a, i, G, I);
      set_surviving_regs(a, i, G, I);
      c = drylh5801iCode(ic);
      ic->generated = false;
      return c;
    }
}

// Only for pruning; never needed with this few registers (as hc08).
template <class G_t, class I_t>
static bool assignment_hopeless(const assignment &a, unsigned short int i, const G_t &G, const I_t &I, const var_t lastvar)
{
  return false;
}

template <class T_t>
static void get_best_local_assignment_biased(assignment &a, typename boost::graph_traits<T_t>::vertex_descriptor t, const T_t &T)
{
  a = *T[t].assignments.begin();

  varset_t newlocal;
  std::set_union(T[t].alive.begin(), T[t].alive.end(), a.local.begin(), a.local.end(), std::inserter(newlocal, newlocal.end()));
  a.local = newlocal;
}

template <class G_t, class I_t>
static float rough_cost_estimate(const assignment &a, unsigned short int i, const G_t &G, const I_t &I)
{
  return 0.0f;
}

// A compare that genCmp() fuses with the IFX after it (ifxForOp()) never
// stores its 0/1 result, and generates that IFX itself; so does a bit
// test genBitwise() fuses (lh5801_bitTest()).
static void extra_ic_generated(iCode *ic)
{
  iCode *bt;
  if ((bt = lh5801_bitTest(ic)))
    {
      OP_SYMBOL(IC_RESULT(ic))->for_newralloc = false;
      OP_SYMBOL(IC_RESULT(ic))->regType = REG_CND;
      bt->generated = true;
      return;
    }
  if (ic->op == '>' || ic->op == '<' || ic->op == LE_OP || ic->op == GE_OP || ic->op == EQ_OP || ic->op == NE_OP)
    {
      iCode *ifx;
      if ((ifx = ifxForOp(IC_RESULT(ic), ic)))
        {
          OP_SYMBOL(IC_RESULT(ic))->for_newralloc = false;
          OP_SYMBOL(IC_RESULT(ic))->regType = REG_CND;
          ifx->generated = true;
        }
    }
}

template <class T_t, class G_t, class I_t>
static bool tree_dec_ralloc(T_t &T, G_t &G, const I_t &I)
{
  bool assignment_optimal;

  con2_t I2(boost::num_vertices(I));
  for (unsigned int i = 0; i < boost::num_vertices(I); i++)
    {
      I2[i].v = I[i].v;
      I2[i].byte = I[i].byte;
      I2[i].size = I[i].size;
      I2[i].name = I[i].name;
    }
  typename boost::graph_traits<I_t>::edge_iterator e, e_end;
  for (boost::tie(e, e_end) = boost::edges(I); e != e_end; ++e)
    add_edge(boost::source(*e, I), boost::target(*e, I), I2);

  assignment ac;
  assignment_optimal = true;
  tree_dec_ralloc_nodes(T, find_root(T), G, I2, ac, &assignment_optimal);

  const assignment &winner = *(T[find_root(T)].assignments.begin());

  if (winner.global.size() != boost::num_vertices(I))
    {
      std::cerr << "lh5801: register allocation found no assignment\n";
      exit(-1);
    }

  for (unsigned int v = 0; v < boost::num_vertices(I); v++)
    {
      symbol *sym = (symbol *)(hTabItemWithKey(liveRanges, I[v].v));
      sym->nRegs = I[v].size;
      sym->accuse = 0;
      sym->isspilt = false;
      sym->regs[I[v].byte] = winner.global[v] >= 0 ? lh5801_regs + winner.global[v] : 0;
    }

  for (unsigned int i = 0; i < boost::num_vertices(G); i++)
    set_surviving_regs(winner, i, G, I);

  return !assignment_optimal;
}

iCode *lh5801_ralloc2_cc(ebbIndex *ebbi)
{
  cfg_t control_flow_graph;
  con_t conflict_graph;

  iCode *ic = create_cfg(control_flow_graph, conflict_graph, ebbi);

  if (optimize.genconstprop)
    recomputeValinfos(ic, ebbi, "_3");

  guessCounts(ic, ebbi);

  tree_dec_t tree_decomposition;
  get_nice_tree_decomposition(tree_decomposition, control_flow_graph);
  alive_tree_dec(tree_decomposition, control_flow_graph);
  good_re_root(tree_decomposition);
  nicify(tree_decomposition);
  alive_tree_dec(tree_decomposition, control_flow_graph);

  guessCounts(ic, ebbi);

  tree_dec_ralloc(tree_decomposition, control_flow_graph, conflict_graph);

  return ic;
}
