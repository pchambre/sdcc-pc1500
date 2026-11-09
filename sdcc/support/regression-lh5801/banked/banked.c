/* banked.c - bank-switched code (2026-11-09): main() is fixed code,
   banked1.c and banked2.c are banks 1 and 2 (build-lh5801.sh --banks=).
   Calls into a bank and between banks, nested three deep and back into
   the first bank, with arguments and results in A, X and memory; the
   bank that's in at each step; a fixed function called from a bank; and
   exit() from inside a bank, which has to put the starting bank back
   (banked.sh checks that afterwards). */
#include "../tests/check.h"
#include <pc1500_bank.h>
#include "banked.h"

volatile unsigned char seen[4];
volatile unsigned char va = 7;
volatile unsigned int vx = 1000;
/* set by banked.sh before a second CALL: not in DATA, which startup zeroes */
__at (0x77F0) volatile unsigned char doExit;

unsigned char fixed_inc (unsigned char a)
{
    seen[3] = bank_current ();
    return a + 1;
}

void main (void)
{
    unsigned char home = bank_current ();

    CHECK (1, b1_bank () == 1);
    CHECK (2, bank_current () == home);
    /* b1_sum: x + a + b2_twice(a) + fixed_inc(a) = 1000 + 7 + 14 + 8 */
    CHECK (3, b1_sum (vx, va) == 1029);
    CHECK (4, seen[0] == 1 && seen[1] == 2 && seen[3] == 1);
    /* b1_back calls b2_twice, and is back in bank 1 after */
    CHECK (5, b1_back (va) == 14 + 1);
    CHECK (6, seen[2] == 2);
    CHECK (7, bank_current () == home);
    CHECK (8, b2_long (0x12345678UL) == 0x12345679UL);
    bank_select (2);
    CHECK (9, bank_current () == 2);
    bank_select (home);
    if (doExit)
        b1_exit ();
    pass ();
}
