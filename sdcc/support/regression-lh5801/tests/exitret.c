/* exit() from a nested call returns to BASIC with its stack intact:
   BASIC then runs the rest of the command line, which run.sh's script
   checks for. Unlike the other tests this one doesn't stay on "OK" -- it
   writes a marker and leaves, and the script checks the marker and BASIC's
   prompt. */
#include "check.h"

volatile unsigned char marker;

void deeper(unsigned char n)
{
    if (n == 3) {
        marker = 0xA5;
        exit(0);
    }
    marker = n;
}

void deep(unsigned char n)
{
    deeper(n);
    deep(n + 1);   /* never returns twice: exit() at n == 3 */
}

int main(void)
{
    deep(1);
    marker = 0x5A;                          /* not reached */
    return 0;
}
