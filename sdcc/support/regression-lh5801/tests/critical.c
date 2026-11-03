/* critical.c - __critical { } blocks: interrupts off inside (IE, flag
   bit 1, clear), and IE back as it was afterwards, whichever way it was;
   a block in a function called from inside another (SDCC allows no block
   directly inside one, nor a return out of one). */

#include "check.h"

volatile unsigned char t;    /* the flags, read by flags() */
volatile unsigned char seen; /* IE inside the callee */

static unsigned char flags(void)
{
    __asm__ ("tta\n\tsta (_t)");
    return t & 0x02;
}

static void ieOn(void) { __asm__ ("sie"); }
static void ieOff(void) { __asm__ ("rie"); }

static void callee(void)
{
    seen = flags();
}

static void inner(void)
{
    __critical {
        seen = flags();
    }
}

void main(void)
{
    unsigned char before = flags();

    ieOn();
    CHECK(1, flags());
    __critical {
        CHECK(2, !flags());
        callee();
        CHECK(3, !seen);
        inner();             /* its own block: off, and its end leaves them off */
        CHECK(4, !seen);
        CHECK(5, !flags());
    }
    CHECK(6, flags()); /* back on */

    ieOff();
    __critical {
        CHECK(7, !flags());
    }
    CHECK(8, !flags()); /* back off: it was off */

    ieOn();
    inner();
    CHECK(9, !seen);
    CHECK(10, flags());

    if (!before)
        ieOff();
    pass();
}
