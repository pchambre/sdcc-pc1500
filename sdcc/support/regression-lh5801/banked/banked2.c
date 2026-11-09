/* banked2.c - the banked-code test's bank 2. */
#pragma bank 2
#include <pc1500_bank.h>
#include "banked.h"

unsigned char b2_twice (unsigned char a) __banked
{
    seen[1] = bank_current ();
    seen[2] = bank_current ();
    return a * 2;
}

unsigned long b2_long (unsigned long v) __banked
{
    return v + 1;
}
