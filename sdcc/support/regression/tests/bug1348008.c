/*
   bug1348008.c
*/

#include <testfwk.h>

#if !defined(PORT_HOST) && !defined(__SDCC_lh5801) /* lh5801: no __interrupt, the PC-1500's ROM owns the vectors */

void
foo (void)
{
}

void
IRQ_ISR (void) __interrupt
{
  foo();
}

#endif

void
testBug (void)
{
}
