/* banked1.c - the banked-code test's bank 1. */
#pragma bank 1
#include <pc1500.h>
#include <pc1500_bank.h>
#include "banked.h"

static unsigned char here (void)
{
    return bank_current ();
}

unsigned int b1_sum (unsigned int x, unsigned char a) __banked
{
    seen[0] = here ();
    return x + a + b2_twice (a) + fixed_inc (a);
}

unsigned char b1_bank (void) __banked
{
    return here ();
}

unsigned char b1_back (unsigned char a) __banked
{
    unsigned char r = b2_twice (a);

    return r + (here () == 1);
}

void b1_exit (void) __banked
{
    exit (0);
}
