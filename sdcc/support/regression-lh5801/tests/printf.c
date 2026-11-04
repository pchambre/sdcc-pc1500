/* printf's formatting (device/lib/lh5801/printf_large.c): the
   conversions, flags, widths and modifiers, and the quirks it keeps from
   SDCC's generic printf_large.c -- a string's padding counts its whole
   length under a precision, %f is <NO FLOAT>. Each case is checked
   through sprintf(), its text and its count. Values go through volatile
   globals so nothing is folded at compile time. */
#include <stdio.h>
#include <string.h>
#include "check.h"

volatile int vi = -123;
volatile int v42 = 42;
volatile unsigned int vu = 65535u;
volatile unsigned int vbeef = 0xBEEFu;
volatile long vl = -2147483647L - 1;
volatile unsigned long vul = 4294967295UL;
volatile unsigned long vhex = 0x12345678UL;
volatile float vf = 1.5f;

static char buf[40];

/* the text and the count sprintf() returned */
static unsigned char is(int n, const char *want)
{
    return n == (int)strlen(want) && !strcmp(buf, want);
}

int main(void)
{
    CHECK(1, is(sprintf(buf, "%d", 0), "0"));
    CHECK(2, is(sprintf(buf, "%d", vi), "-123"));
    CHECK(3, is(sprintf(buf, "%5d|%-5d|", v42, v42), "   42|42   |"));
    CHECK(4, is(sprintf(buf, "%05d", -v42), "-0042"));
    CHECK(5, is(sprintf(buf, "%+d % d", v42, v42), "+42  42"));
    CHECK(6, is(sprintf(buf, "%u %d %d", vu, 32767, -32767 - 1), "65535 32767 -32768"));
    CHECK(7, is(sprintf(buf, "%x %X %o %x", vbeef, vbeef, 8, 0), "beef BEEF 10 0"));
    CHECK(8, is(sprintf(buf, "%ld", vl), "-2147483648"));
    CHECK(9, is(sprintf(buf, "%lu %lx %lX", vul, vhex, vhex), "4294967295 12345678 12345678"));
    CHECK(10, is(sprintf(buf, "%c%c%s", 'A', 'b', "cd"), "Abcd"));
    CHECK(11, is(sprintf(buf, "%5s|%-5s|", "hi", "hi"), "   hi|hi   |"));
    CHECK(12, is(sprintf(buf, "%.2s|%5.2s|", "hello", "hello"), "he|he|"));    /* padding counts 5 */
    CHECK(13, is(sprintf(buf, "%d%%", v42), "42%"));
    CHECK(14, is(sprintf(buf, "[%f]", vf), "[<NO FLOAT>]"));
    CHECK(15, is(sprintf(buf, "%p %P", (void *)0x12AB, (void *)0x12AB), "0x12ab 0x12AB"));
    CHECK(16, is(sprintf(buf, "%3d|%1d|%08lx", 12345, 7, 0xABCDL), "12345|7|0000abcd"));
    CHECK(17, is(sprintf(buf, "%hd %zu %D %U", v42, 7, v42, 9), "42 7 42 9"));
    CHECK(18, is(sprintf(buf, "%-+6d|%- 6d|", v42, v42), "+42   | 42   |"));
    CHECK(19, is(sprintf(buf, "%06x|%-06x|", 255, 255), "0000ff|ff    |"));
    CHECK(20, is(sprintf(buf, "%k%d", 5), "K5"));        /* unknown: printed, upper case */
    CHECK(21, is(sprintf(buf, "%lX %lx %X", 0xABCDEF01L, 0xABCDEF01L, 0xABCu), "ABCDEF01 abcdef01 ABC"));
    {
        static char longText[301];
        static char big[310];

        memset(longText, 'a', 300);
        longText[299] = 'z';
        CHECK(22, sprintf(big, "<%s>", longText) == 302 && big[300] == 'z' && big[301] == '>' && !big[302]);
    }
    pass();
    return 0;
}
