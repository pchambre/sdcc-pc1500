/* 32-bit arithmetic, comparisons and shifts, and widening between sizes
   (sign- vs zero-extension). */
#include "check.h"

volatile unsigned long ua = 3000000000ul, ub = 123456ul;
volatile long sa = -2000000000l, sb = 77777l;
volatile signed char sc = -7;
volatile unsigned char uc = 249;
volatile int si = -1000;
volatile unsigned int ui = 65000u;

int main(void)
{
    unsigned long u;
    long s;
    int i;

    CHECK(1, ua + ub == 3000123456ul);
    CHECK(2, ub - ua == 1295090752ul);
    CHECK(3, sa + sb == -1999922223l);
    CHECK(4, ua > ub && sa < sb && sa < 0 && ua > 0x7FFFFFFFul);
    CHECK(5, (ua & 0xFFFF0000ul) == 0xB2D00000ul && (ub | 1ul) == 123457ul);
    u = ua;
    u >>= 24;
    CHECK(6, u == 0xB2);
    u = ub;
    u <<= 16;
    CHECK(7, u == 0xE2400000ul);
    u = ub;
    u <<= 3;
    CHECK(8, u == 987648ul);
    s = sa;
    s >>= 20;
    CHECK(9, s == -1908);
    s = -sb;
    CHECK(10, s == -77777l);
    CHECK(11, ub * 100ul == 12345600ul);
    CHECK(12, ua / 1000ul == 3000000ul && ub % 1000ul == 456ul);
    CHECK(13, sa / 3l == -666666666l && sa % 3l == -2l);
    /* widening */
    s = sc;
    CHECK(14, s == -7l);
    s = uc;
    CHECK(15, s == 249l);
    s = si;
    CHECK(16, s == -1000l);
    s = ui;
    CHECK(17, s == 65000l);
    i = sc;
    CHECK(18, i == -7);
    i = uc;
    CHECK(19, i == 249);
    s = -1;
    CHECK(20, s == -1l && (unsigned long)s == 0xFFFFFFFFul);
    /* narrowing */
    i = (int)sa;
    CHECK(21, i == 27648);                  /* -2000000000 & 0xFFFF = 0x6C00 */
    CHECK(22, (unsigned char)ua == 0x00 && (unsigned char)ub == 0x40);
    pass();
    return 0;
}
