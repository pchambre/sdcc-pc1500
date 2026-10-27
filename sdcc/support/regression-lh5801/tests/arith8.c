/* 8-bit arithmetic, comparisons, bitwise ops and shifts, signed and
   unsigned, at the boundaries. */
#include "check.h"

volatile unsigned char ua = 200, ub = 55, uz = 0, ux = 0x80;
volatile signed char sa = -100, sb = 27, sm = -128, sp = 127;
volatile unsigned char n3 = 3;

int main(void)
{
    unsigned char u;
    signed char s;

    CHECK(1, (unsigned char)(ua + ub) == 255);
    CHECK(2, (unsigned char)(ua + ua) == 144);
    CHECK(3, (unsigned char)(ub - ua) == 111);
    CHECK(4, (signed char)(sa + sb) == -73);
    CHECK(5, (signed char)(sm - 1) == 127);
    CHECK(6, ua > ub && ub < ua && ua >= 200 && ub <= 55 && ua != ub);
    CHECK(7, !(ua < ub) && !(uz > 0));
    CHECK(8, sa < sb && sm < sa && sp > sb && sa <= -100 && sb >= 27);
    CHECK(9, sm < 0 && sp > 0 && !(sa > 0));
    CHECK(10, ux > 0x7F);                   /* unsigned: 0x80 is big */
    CHECK(11, (ua & ub) == 0 && (ua | ub) == 0xFF && (ua ^ ub) == 0xFF && (ua & 0xF0) == 0xC0);
    CHECK(12, (unsigned char)~ua == 55);
    u = ua;
    u >>= 3;
    CHECK(13, u == 25);
    u = ub;
    u <<= 2;
    CHECK(14, u == 220);
    s = sa;
    s >>= 2;
    CHECK(15, s == -25);                    /* arithmetic shift */
    u = ua >> n3;                           /* runtime count */
    CHECK(16, u == 25);
    s = -sa;
    CHECK(17, s == 100);
    CHECK(18, (unsigned char)(ua * 3) == 88);
    CHECK(19, ua / 7 == 28 && ua % 7 == 4);
    CHECK(20, sa / 7 == -14 && sa % 7 == -2);
    u = 255;
    u++;
    CHECK(21, u == 0);
    u--;
    CHECK(22, u == 255);
    pass();
    return 0;
}
