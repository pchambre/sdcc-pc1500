/*-------------------------------------------------------------------------
  outline.c - outlining repeated code into subroutines for the LH5801 port.

  Paul Chambre, 2026

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  Code generation is local: the same few instructions -- an array
  element's address, a constant into a variable, a parameter's setup --
  come out again and again across a module. Each such sequence can live
  once, as a subroutine, with every copy a 3-byte SJP to it ("cross-call"
  optimization in 8-bit embedded compilers, LLVM's machine outliner).

  On the LH5801 that is safe for any straight-line run: SJP and RTN change
  no register and no flag, and the sequence does exactly what it did in
  place. A run ends at a label, a branch or jump, a return, anything using
  the stack (PSH/POP, LDX/STX, a call while arguments are pushed for it --
  its callee finds them past its return address), a reference to a
  function's own NNNNN$ labels (scoped to it) or inline asm; and a 32-bit
  helper's SJP
  keeps its inline .dw/.db operands (gen.c) -- inside the subroutine they
  still follow it. A sequence may end in the function's RTN: then each
  copy is a JMP to the subroutine, whose RTN returns for the function.
  Every sequence taken is longer than the SJP replacing
  it, so code only shrinks and short branches stay in range.

  Function code is held back instead of printed (lh5801_deferCode(), from
  gen.c's genLH5801Code()) until the module is compiled; SDCCglue.c's
  glue() then calls lh5801_flushCode() before it adds anything else to
  the code buffer: the repeats are found across all of the module's
  functions, greedily -- the sequence saving the most bytes first, then
  again among what is left -- each copy is replaced, and the subroutines
  follow the functions. --lh5801-no-outline or --opt-code-speed (a call
  and return per copy is slower) leave the code as it was.
-------------------------------------------------------------------------*/

#include "common.h"
#include "ralloc.h"
#include "gen.h"

bool lh5801_noOutline = false;

#define MAXLEN 32               /* instructions in one sequence */

typedef struct chunk
{
  lineNode *head;
  struct dbuf_s *buf;
} chunk;

static chunk *chunks;
static int nChunks;

/* gen.c's genLH5801Code(): keep a function's lines (or GSINIT's) for
   lh5801_flushCode() to print to `buf`. */
void
lh5801_deferCode (lineNode *head, struct dbuf_s *buf)
{
  if (!head)
    return;
  chunks = Safe_realloc (chunks, (nChunks + 1) * sizeof (*chunks));
  chunks[nChunks].head = head;
  chunks[nChunks].buf = buf;
  nChunks++;
}

/* One instruction of the module's code. */
typedef struct ins
{
  lineNode *ln;
  int chunk;
  int id;                       /* same text, same id */
  int size;
  int block;                    /* straight-line run */
  bool data;                    /* a helper's .dw/.db */
  bool rtn;                     /* RTN: ends its run, may end a sequence */
  bool used;                    /* already outlined */
} ins;

static ins *code_;
static int nIns;

/* line text -> id */
static char **texts;
static int nTexts;
static int *textHash;
static int textHashSize;

static unsigned int
strHash (const char *s)
{
  unsigned int h = 5381;

  while (*s)
    h = h * 33 + (unsigned char) *s++;
  return h;
}

static void
growTextHash (void)
{
  int i;

  Safe_free (textHash);
  textHashSize = textHashSize ? 2 * textHashSize : 4096;
  textHash = Safe_malloc (textHashSize * sizeof (int));
  for (i = 0; i < textHashSize; i++)
    textHash[i] = -1;
  for (i = 0; i < nTexts; i++)
    {
      unsigned int h = strHash (texts[i]) & (textHashSize - 1);

      while (textHash[h] >= 0)
        h = (h + 1) & (textHashSize - 1);
      textHash[h] = i;
    }
}

static int
textId (const char *s)
{
  unsigned int h;

  if (2 * (nTexts + 1) > textHashSize)
    growTextHash ();
  h = strHash (s) & (textHashSize - 1);
  while (textHash[h] >= 0)
    {
      if (!strcmp (texts[textHash[h]], s))
        return textHash[h];
      h = (h + 1) & (textHashSize - 1);
    }
  texts = Safe_realloc (texts, (nTexts + 1) * sizeof (*texts));
  texts[nTexts] = Safe_strdup (s);
  textHash[h] = nTexts;
  return nTexts++;
}

static const char *
trimmed (const char *l)
{
  while (*l == ' ' || *l == '\t')
    l++;
  return l;
}

/* Can `l` (trimmed) go into a sequence? Else it ends the run. */
static bool
movable (const char *l)
{
  static const char *const stoppers[] = {
    "jmp", "rti", "lop", "psh", "pop", "ldx", "stx", "hlt", "off", NULL
  };
  char mn[8];
  size_t n = strcspn (l, "\t ");
  int k;

  if (n >= sizeof (mn))
    return false;
  memcpy (mn, l, n);
  mn[n] = 0;
  if (mn[0] == 'b' && strcmp (mn, "bii"))
    return false;               /* branches */
  for (k = 0; stoppers[k]; k++)
    if (!strcmp (stoppers[k], mn))
      return false;
  return !strchr (l + n, '$');  /* a function's NNNNN$ labels are its own */
}

/* Does any instruction of the function starting at `head` name the
   numbered label `def` ("NNNNN$:")? */
static bool
labelUsed (lineNode *head, const char *def)
{
  char name[16];
  size_t n = strlen (def) - 1;  /* "NNNNN$" */
  lineNode *ln;

  if (n >= sizeof (name))
    return true;
  memcpy (name, def, n);
  name[n] = 0;
  for (ln = head; ln; ln = ln->next)
    {
      const char *p;

      if (!ln->line || ln->isComment)
        continue;
      for (p = ln->line; (p = strstr (p, name)) != NULL; p++)
        if ((p == ln->line || !isdigit ((unsigned char) p[-1])) && p[n] != ':')
          return true;
    }
  return false;
}

static void
collect (void)
{
  int c, block = 0, pushed;

  for (c = 0; c < nChunks; c++)
    {
      lineNode *ln;

      if (chunks[c].buf != &code->oBuf)
        continue;
      block++;
      pushed = 0;
      for (ln = chunks[c].head; ln; ln = ln->next)
        {
          const char *l = ln->line ? trimmed (ln->line) : "";
          bool data;

          if (ln->isComment || !*l || *l == ';')
            continue;           /* comments: dropped with what they're in */
          if (l[strlen (l) - 1] == ':' && isdigit ((unsigned char) l[0]) && !labelUsed (chunks[c].head, l))
            continue;           /* so is a label nothing jumps to (a lone
                                   RTN's exit label) */
          /* bytes pushed for a call's stack arguments (reentrant, varargs):
             that call can't move -- in a subroutine, its return address
             would come between the arguments and the callee */
          if (!strncmp (l, "psh", 3))
            pushed += l[4] == 'a' ? 1 : 2;
          else if (!strncmp (l, "pop", 3))
            pushed -= l[4] == 'a' ? 1 : 2;
          data = !strncmp (l, ".dw", 3) || !strncmp (l, ".db", 3);
          if (ln->isLabel || ln->isInline || l[strlen (l) - 1] == ':' || (*l == '.' && !data) ||
              (!data && !movable (l)) || (pushed > 0 && !strncmp (l, "sjp", 3)))
            {
              block++;
              continue;
            }
          code_ = Safe_realloc (code_, (nIns + 1) * sizeof (*code_));
          code_[nIns].ln = ln;
          code_[nIns].chunk = c;
          code_[nIns].id = textId (l);
          code_[nIns].size = lh5801_getInstructionSize (ln);
          code_[nIns].block = block;
          code_[nIns].data = data;
          code_[nIns].rtn = !strncmp (l, "rtn", 3);
          code_[nIns].used = false;
          nIns++;
          if (code_[nIns - 1].rtn)
            block++;            /* nothing follows a return in its run */
        }
    }
}

/* All the places one sequence occurs, in order. */
typedef struct cand
{
  unsigned int hash;
  int len, size;
  int *starts;
  int nStarts;
} cand;

static bool
sameSeq (int a, int b, int len)
{
  int k;

  for (k = 0; k < len; k++)
    if (code_[a + k].id != code_[b + k].id)
      return false;
  return true;
}

/* The best sequence left to outline: its first instruction, length and
   the (non-overlapping) starts of its copies; false if none saves bytes. */
static bool
bestSeq (int *bestLen, int **bestStarts, int *bestN)
{
  int tableSize = 1, nCands = 0, s, len, i;
  int *table;
  cand *cands = NULL;
  int bestGain = 0;

  while (tableSize < 4 * nIns * MAXLEN)
    tableSize <<= 1;
  table = Safe_alloc (tableSize * sizeof (int));
  for (i = 0; i < tableSize; i++)
    table[i] = -1;

  for (s = 0; s < nIns; s++)
    {
      unsigned int h = 0;
      int size = 0;

      if (code_[s].used || code_[s].data)
        continue;
      for (len = 1; len <= MAXLEN && s + len <= nIns; len++)
        {
          int e = s + len - 1;
          unsigned int slot;

          if (code_[e].used || code_[e].block != code_[s].block)
            break;
          h = h * 1000003u + (unsigned int) code_[e].id;
          size += code_[e].size;
          /* a helper's SJP keeps its operands */
          if (e + 1 < nIns && code_[e + 1].data && code_[e + 1].block == code_[s].block)
            continue;
          if (size <= 3)
            continue;
          slot = (h ^ (unsigned int) len * 2654435761u) & (tableSize - 1);
          while (table[slot] >= 0)
            {
              cand *c = &cands[table[slot]];

              if (c->hash == h && c->len == len && sameSeq (c->starts[0], s, len))
                break;
              slot = (slot + 1) & (tableSize - 1);
            }
          if (table[slot] < 0)
            {
              cands = Safe_realloc (cands, (nCands + 1) * sizeof (*cands));
              cands[nCands].hash = h;
              cands[nCands].len = len;
              cands[nCands].size = size;
              cands[nCands].starts = NULL;
              cands[nCands].nStarts = 0;
              table[slot] = nCands++;
            }
          {
            cand *c = &cands[table[slot]];

            /* copies may not overlap */
            if (c->nStarts && c->starts[c->nStarts - 1] + len > s)
              continue;
            c->starts = Safe_realloc (c->starts, (c->nStarts + 1) * sizeof (int));
            c->starts[c->nStarts++] = s;
          }
        }
    }

  *bestN = 0;
  for (i = 0; i < nCands; i++)
    {
      int n = cands[i].nStarts, b = cands[i].size;
      /* ending in RTN, each copy is a JMP there and the body needs no RTN
         of its own */
      bool tail = code_[cands[i].starts[0] + cands[i].len - 1].rtn;
      int gain = n * b - (3 * n + b + !tail);

      if (n >= 2 && gain > bestGain)
        {
          bestGain = gain;
          *bestLen = cands[i].len;
          Safe_free (*bestStarts);
          *bestStarts = Safe_malloc (n * sizeof (int));
          memcpy (*bestStarts, cands[i].starts, n * sizeof (int));
          *bestN = n;
        }
    }

  for (i = 0; i < nCands; i++)
    Safe_free (cands[i].starts);
  Safe_free (cands);
  Safe_free (table);
  return bestGain > 0;
}

/* One subroutine: its body's lines. */
typedef struct sub
{
  char **lines;
  int nLines;
  bool tail;                    /* ends in its own RTN */
} sub;

static sub *subs;
static int nSubs;

static void
freeLines (lineNode *ln, lineNode *stop)
{
  while (ln != stop)
    {
      lineNode *next = ln->next;

      if (ln->line)
        Safe_free (ln->line);
      if (ln->aln)
        Safe_free (ln->aln);
      Safe_free (ln);
      ln = next;
    }
}

/* Replace each copy of the `len` instructions at `starts` by an SJP to
   subroutine number nSubs, whose body is the first copy's text. */
static void
outlineSeq (int len, const int *starts, int n)
{
  char call[64];
  sub *sb;
  int i, k;

  subs = Safe_realloc (subs, (nSubs + 1) * sizeof (*subs));
  sb = &subs[nSubs];
  sb->lines = Safe_malloc (len * sizeof (char *));
  sb->nLines = len;
  sb->tail = code_[starts[0] + len - 1].rtn;
  for (k = 0; k < len; k++)
    sb->lines[k] = Safe_strdup (trimmed (code_[starts[0] + k].ln->line));
  snprintf (call, sizeof (call), "%s\t__lh5801_ol%d", sb->tail ? "jmp" : "sjp", nSubs);
  nSubs++;

  for (i = 0; i < n; i++)
    {
      lineNode *first = code_[starts[i]].ln;
      lineNode *last = code_[starts[i] + len - 1].ln;
      lineNode *nl = newLineNode (call);
      chunk *c = &chunks[code_[starts[i]].chunk];

      nl->ic = first->ic;
      nl->prev = first->prev;
      nl->next = last->next;
      if (first->prev)
        first->prev->next = nl;
      else
        c->head = nl;
      if (last->next)
        last->next->prev = nl;
      last->next = NULL;
      for (k = 0; k < len; k++)
        code_[starts[i] + k].used = true;
      freeLines (first, NULL);
    }
}

/* A call right before the function's RTN -- perhaps past its exit label,
   when nothing else goes there (peeph.def's labelRefCount() doesn't see
   that) -- is a tail call: JMP there, and its RTN returns for both. */
static void
tailCalls (lineNode **head)
{
  lineNode *ln;

  for (ln = *head; ln; ln = ln->next)
    {
      lineNode *r = ln->next;

      if (!ln->line || ln->isInline || strncmp (trimmed (ln->line), "sjp\t", 4))
        continue;
      while (r && r->line && !r->isInline &&
             (r->isComment || *trimmed (r->line) == ';' ||
              (isdigit ((unsigned char) *trimmed (r->line)) && trimmed (r->line)[strlen (trimmed (r->line)) - 1] == ':' &&
               !labelUsed (*head, trimmed (r->line)))))
        r = r->next;
      if (!r || !r->line || r->isInline || strcmp (trimmed (r->line), "rtn"))
        continue;
      {
        char *jmp = Safe_strdup (trimmed (ln->line));

        memcpy (jmp, "jmp", 3);
        Safe_free (ln->line);
        ln->line = jmp;
      }
      /* drop what came between, and the RTN */
      {
        lineNode *after = r->next;

        r->next = NULL;
        freeLines (ln->next, NULL);
        ln->next = after;
        if (after)
          after->prev = ln;
      }
    }
}

/* SDCCglue.c's glue(), before anything else goes into the code buffer. */
void
lh5801_flushCode (void)
{
  int c, i;

  if (!lh5801_noOutline && !optimize.codeSpeed)
    {
      int len = 0, n = 0;
      int *starts = NULL;

      for (c = 0; c < nChunks; c++)
        if (chunks[c].buf == &code->oBuf)
          tailCalls (&chunks[c].head);
      collect ();
      while (nIns && bestSeq (&len, &starts, &n))
        outlineSeq (len, starts, n);
      Safe_free (starts);
    }

  for (c = 0; c < nChunks; c++)
    {
      printLine (chunks[c].head, chunks[c].buf);
      freeLines (chunks[c].head, NULL);
    }
  for (i = 0; i < nSubs; i++)
    {
      int k;

      dbuf_printf (&code->oBuf, "__lh5801_ol%d:\n", i);
      for (k = 0; k < subs[i].nLines; k++)
        {
          dbuf_printf (&code->oBuf, "\t%s\n", subs[i].lines[k]);
          Safe_free (subs[i].lines[k]);
        }
      if (!subs[i].tail)
        dbuf_printf (&code->oBuf, "\trtn\n");
      Safe_free (subs[i].lines);
    }

  Safe_free (chunks);
  chunks = NULL;
  nChunks = 0;
  Safe_free (subs);
  subs = NULL;
  nSubs = 0;
  Safe_free (code_);
  code_ = NULL;
  nIns = 0;
  for (i = 0; i < nTexts; i++)
    Safe_free (texts[i]);
  Safe_free (texts);
  texts = NULL;
  nTexts = 0;
  Safe_free (textHash);
  textHash = NULL;
  textHashSize = 0;
}
