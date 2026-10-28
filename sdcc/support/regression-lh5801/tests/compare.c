/* Comparisons of 16- and 32-bit values, as branches and as values: all
   six operators against literals (the largest value, where > and <= can't
   become >= and < of k+1; values with zero low bytes, which the carry
   chain skips; signed boundaries) and against variables (where > and <=
   swap their operands). */
#include "check.h"

volatile long sl, sl2;
volatile unsigned long ul, ul2;
volatile int si, si2;
volatile unsigned int ui, ui2;
volatile unsigned char r;

/* all six operators as branches: expected results for <, <=, >, >=, ==, != */
#define T6(n, a, b, lt, le, gt, ge, eq)          \
    do {                                         \
        CHECK((n), ((a) < (b)) == (lt));         \
        CHECK((n) + 1, ((a) <= (b)) == (le));    \
        CHECK((n) + 2, ((a) > (b)) == (gt));     \
        CHECK((n) + 3, ((a) >= (b)) == (ge));    \
        CHECK((n) + 4, ((a) == (b)) == (eq));    \
        CHECK((n) + 5, ((a) != (b)) == !(eq));   \
    } while (0)

/* a comparison kept as a value */
#define V(n, e, want)                            \
    do {                                         \
        r = (e);                                 \
        CHECK((n), r == (want));                 \
    } while (0)

int main(void)
{
    /* signed 32-bit against literals */
    sl = 5;
    T6(1, sl, 4, 0, 0, 1, 1, 0);
    T6(7, sl, 5, 0, 1, 0, 1, 1);
    T6(13, sl, 6, 1, 1, 0, 0, 0);
    sl = 0x10000;
    T6(19, sl, 0x10000, 0, 1, 0, 1, 1);
    T6(25, sl, 0xFFFF, 0, 0, 1, 1, 0);
    T6(31, sl, 0x10001, 1, 1, 0, 0, 0);
    T6(37, sl, 0x20000, 1, 1, 0, 0, 0);
    sl = -1;
    T6(43, sl, 0, 1, 1, 0, 0, 0);
    T6(49, sl, -1, 0, 1, 0, 1, 1);
    T6(55, sl, -2, 0, 0, 1, 1, 0);
    sl = 0;
    T6(61, sl, 0, 0, 1, 0, 1, 1);
    sl = -2147483647L - 1;
    T6(67, sl, 2147483647L, 1, 1, 0, 0, 0);
    T6(73, sl, -2147483647L - 1, 0, 1, 0, 1, 1);
    sl = 2147483647L;
    T6(79, sl, 2147483647L, 0, 1, 0, 1, 1);
    T6(85, sl, -2147483647L - 1, 0, 0, 1, 1, 0);
    sl = 0x12345678;
    T6(91, sl, 0x12345679, 1, 1, 0, 0, 0);
    T6(97, sl, 0x02345678, 0, 0, 1, 1, 0);

    /* unsigned 32-bit against literals */
    ul = 0xFFFFFFFFUL;
    T6(103, ul, 0xFFFFFFFFUL, 0, 1, 0, 1, 1);
    T6(109, ul, 0xFFFFFFFEUL, 0, 0, 1, 1, 0);
    ul = 0x80000000UL;
    T6(115, ul, 0x7FFFFFFFUL, 0, 0, 1, 1, 0);
    T6(121, ul, 0x80000000UL, 0, 1, 0, 1, 1);

    /* 32-bit against variables (> and <= swap their operands) */
    sl = -3; sl2 = 2;
    T6(127, sl, sl2, 1, 1, 0, 0, 0);
    T6(133, sl2, sl, 0, 0, 1, 1, 0);
    sl2 = -3;
    T6(139, sl, sl2, 0, 1, 0, 1, 1);
    ul = 0x80000000UL; ul2 = 0x7FFFFFFFUL;
    T6(145, ul, ul2, 0, 0, 1, 1, 0);
    T6(151, ul2, ul, 1, 1, 0, 0, 0);

    /* 16-bit */
    si = -1; si2 = 1;
    T6(157, si, si2, 1, 1, 0, 0, 0);
    T6(163, si, 1, 1, 1, 0, 0, 0);
    si = 32767;
    T6(169, si, 32767, 0, 1, 0, 1, 1);
    T6(175, si, 0x100, 0, 0, 1, 1, 0);
    ui = 0xFFFF; ui2 = 0x8000;
    T6(181, ui, 0xFFFF, 0, 1, 0, 1, 1);
    T6(187, ui, ui2, 0, 0, 1, 1, 0);
    T6(193, ui2, 0x7FFF, 0, 0, 1, 1, 0);

    /* as values */
    sl = 7; sl2 = 9; ul = 0x10000UL;
    V(199, sl > sl2, 0);
    V(200, sl <= sl2, 1);
    V(201, sl > 6, 1);
    V(202, sl <= 6, 0);
    V(203, sl == 7, 1);
    V(204, sl != 7, 0);
    V(205, ul >= 0x10000UL, 1);
    V(206, ul < 0x10000UL, 0);
    V(207, ul > 0xFFFFUL, 1);
    V(208, sl < 0, 0);
    sl = -7;
    V(209, sl < 0, 1);
    V(210, sl >= 0, 0);
    V(211, 3 < sl2, 1);
    pass();
    return 0;
}
