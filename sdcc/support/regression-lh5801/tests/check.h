/* check.h - the LH5801 regression tests' tiny harness.

   Each test is a program that runs its checks and then shows "OK", or
   "FAIL n" for the first check n that failed, and stays there (run.sh's
   pc1500runner script waits for "OK"). Inputs go through volatile
   globals so SDCC can't fold a check away at compile time. */

#ifndef CHECK_H
#define CHECK_H

#include <pc1500.h>

static void stay(void)
{
    for (;;)
        idleTick();
}

static void failAt(unsigned char n)
{
    lcdClear();
    lcdPutString("FAIL ");
    lcdPutChar('0' + n / 100);
    lcdPutChar('0' + n / 10 % 10);
    lcdPutChar('0' + n % 10);
    stay();
}

static void pass(void)
{
    lcdClear();
    lcdPutString("OK");
    stay();
}

#define CHECK(n, cond) do { if (!(cond)) failAt(n); } while (0)

#endif
