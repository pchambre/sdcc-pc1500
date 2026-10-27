/* 16-bit arithmetic, comparisons, bitwise ops and shifts, signed and
   unsigned, at the boundaries. */
#include "check.h"

volatile unsigned int ua = 40000, ub = 1234, ux = 0x8000;
volatile int sa = -30000, sb = 2000, sm = -32768, sp = 32767;
volatile unsigned char n5 = 5, n9 = 9;

int main(void)
{
    unsigned int u;
    int s;

    CHECK(1, ua + ub == 41234u);
    CHECK(2, (unsigned int)(ua + ua) == 14464u);
    CHECK(3, ub - ua == 26770u);
    CHECK(4, sa + sb == -28000);
    CHECK(5, (int)(sm - 1) == 32767);
    CHECK(6, ua > ub && ub < ua && ua >= 40000u && ub <= 1234u && ua != ub);
    CHECK(7, sa < sb && sm < sa && sp > sb && sa <= -30000 && sb >= 2000);
    CHECK(8, sm < 0 && sp > 0 && !(sa > 0) && ux > 0x7FFFu);
    CHECK(9, (ua & ub) == 0x0440u && (ua | ub) == 0x9CD2u && (ua ^ ub) == 0x9892u);
    CHECK(10, (unsigned int)~ub == 0xFB2Du);
    u = ua;
    u >>= 8;
    CHECK(11, u == 156);
    u = ua;
    u >>= 9;
    CHECK(12, u == 78);
    u = ub;
    u <<= 8;
    CHECK(13, u == 0xD200u);
    u = ub;
    u <<= 3;
    CHECK(14, u == 9872u);
    s = sa;
    s >>= 4;
    CHECK(15, s == -1875);
    s = sa;
    s >>= 9;
    CHECK(16, s == -59);                    /* -30000 >> 9 rounds down */
    u = ua >> n5;
    CHECK(17, u == 1250);
    u = ub << n9;
    CHECK(18, u == 0xA400u);
    s = -sa;
    CHECK(19, s == 30000);
    CHECK(20, (unsigned int)(ub * 33u) == 40722u);
    CHECK(21, sb * -3 == -6000 && sb * 7 == 14000);
    CHECK(22, ua / 123u == 325u && ua % 123u == 25u);
    CHECK(23, sa / 7 == -4285 && sa % 7 == -5);
    CHECK(24, ub * 4 == 4936);              /* inline x4 */
    u = 0xFFFF;
    u++;
    CHECK(25, u == 0);
    pass();
    return 0;
}
