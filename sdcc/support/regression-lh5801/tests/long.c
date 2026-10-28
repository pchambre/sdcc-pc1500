/* 32-bit copies, constant stores, adds and subtracts, which go through
   the runtime helpers (device/lib/lh5801/_lh5801_{cpy4,set4,add4,
   sub4}.asm) with every arrangement of operands: in place, into a third
   variable, the result being the right operand, aliasing itself; carries
   and borrows through all four bytes; and values in registers that must
   survive (or keep the inline code). */
#include "check.h"

volatile long a, b, c;
volatile unsigned long ua, ub;
long arr[4];

static long sum(const long *p, unsigned char n)
{
    long s = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        s += p[i];
    return s;
}

int main(void)
{
    volatile long x, y, z;
    unsigned char i;

    /* constant stores */
    x = 0x12345678L;
    CHECK(1, x == 0x12345678L);
    x = 0;
    CHECK(2, x == 0);
    x = -1;
    CHECK(3, x == -1);
    x = 0x01000000L;
    CHECK(4, x == 0x01000000L);

    /* copies */
    a = 0x0BADF00DL;
    x = a;
    y = x;
    CHECK(5, y == 0x0BADF00DL);
    b = y;
    CHECK(6, b == 0x0BADF00DL);

    /* in place: a += b, a -= b */
    a = 0x00FFFFFFL;
    b = 1;
    a = a + b;
    CHECK(7, a == 0x01000000L);
    a = a - b;
    CHECK(8, a == 0x00FFFFFFL);
    ua = 0xFFFFFFFFUL;
    ub = 1;
    ua = ua + ub;
    CHECK(9, ua == 0);
    ua = ua - ub;
    CHECK(10, ua == 0xFFFFFFFFUL);

    /* into a third variable */
    a = 100000L;
    b = -300000L;
    c = a + b;
    CHECK(11, c == -200000L);
    c = a - b;
    CHECK(12, c == 400000L);
    c = b - a;
    CHECK(13, c == -400000L);

    /* the result is the right operand */
    a = 7;
    b = 0x10000L;
    b = a + b;
    CHECK(14, b == 0x10007L);
    b = a - b;
    CHECK(15, b == -0x10000L);

    /* aliasing itself */
    a = 0x40000001L;
    a = a + a;
    CHECK(16, a == 0x80000002L);
    a = a - a;
    CHECK(17, a == 0);

    /* locals */
    x = 0x7FFFFFFFL;
    y = 1;
    z = x + y;
    CHECK(18, z == (long) 0x80000000UL);
    z = z - y;
    CHECK(19, z == 0x7FFFFFFFL);

    /* a loop with a pointer and a counter in registers around the adds */
    for (i = 0; i < 4; i++)
        arr[i] = 0x01010101L * (i + 1);
    CHECK(20, sum(arr, 4) == 0x0A0A0A0AL);
    x = 0;
    for (i = 0; i < 4; i++) {
        y = arr[i];
        x = x + y;
        x = x - 1;
    }
    CHECK(21, x == 0x0A0A0A06L);

    /* shifts: in place, into another variable, signed and unsigned */
    x = 0x40000001L;
    x <<= 1;
    CHECK(22, x == (long) 0x80000002UL);
    y = 0x00012345L;
    x = y << 3;
    CHECK(23, x == 0x00091A28L);
    x = y << 12;
    CHECK(24, x == 0x12345000L);
    x = -0x100L;
    x >>= 3;
    CHECK(25, x == -0x20L);
    x = -1;
    x = x >> 7;
    CHECK(26, x == -1);
    ua = 0x80000000UL;
    ua >>= 5;
    CHECK(27, ua == 0x04000000UL);
    ub = ua >> 9;
    CHECK(28, ub == 0x00020000UL);
    ua = 0xF0000000UL;
    ub = ua >> 1;
    CHECK(29, ub == 0x78000000UL);
    a = (long) 0xF0000000UL;
    b = a >> 1;
    CHECK(30, b == (long) 0xF8000000UL);
    a = 1;
    a = a << 31;
    CHECK(31, a == (long) 0x80000000UL);

    /* negation, in place and into another variable */
    a = 0x00010000L;
    a = -a;
    CHECK(32, a == -0x00010000L);
    b = -a;
    CHECK(33, b == 0x00010000L);
    a = 0;
    a = -a;
    CHECK(34, a == 0);
    pass();
    return 0;
}
