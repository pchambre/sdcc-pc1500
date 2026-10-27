/* In-place updates of array elements and struct members at constant
   addresses (gen.c's genMemUpdate()), and of plain variables
   (genAddSubInPlace()): carries and borrows across bytes, wrap-around,
   masks, a constant the ADI chain can't take, and updates whose old value
   is used, which must not be fused. */
#include "check.h"

unsigned char cs[4];
unsigned int is[4];
long ls[3];
struct {
    unsigned char a;
    int b;
} st;
unsigned char c;
unsigned int u;
long l;

int main(void)
{
    unsigned char old;
    unsigned int oldu;

    cs[2] = 255;
    cs[2]++;
    CHECK(1, cs[2] == 0 && cs[1] == 0 && cs[3] == 0);
    cs[2] -= 2;
    CHECK(2, cs[2] == 254);
    is[1] = 0x00FF;
    is[1]++;
    CHECK(3, is[1] == 0x0100);              /* carry into the high byte */
    is[2] = 2;
    is[2] -= 3;
    CHECK(4, is[2] == 0xFFFF);              /* borrow through the high byte */
    is[2] += 0x0123;                        /* not an ADI chain: high byte isn't 0/FF */
    CHECK(5, is[2] == 0x0122);
    ls[1] = 0x00FFFFFEl;
    ls[1] += 5;
    CHECK(6, ls[1] == 0x01000003l && ls[0] == 0 && ls[2] == 0);
    ls[1] -= 4;
    CHECK(7, ls[1] == 0x00FFFFFFl);         /* borrow up three bytes */
    st.b = -1;
    st.b++;
    CHECK(8, st.b == 0 && st.a == 0);
    cs[3] = 0x81;
    cs[3] |= 0x40;
    cs[3] &= 0xFE;
    CHECK(9, cs[3] == 0xC0);
    is[3] = 0x1234;
    is[3] &= 0xFF0F;
    is[3] |= 0x8000;
    CHECK(10, is[3] == 0x9204);
    old = cs[0]++;                          /* the old value is used: not fused */
    CHECK(11, old == 0 && cs[0] == 1);
    oldu = is[0]--;
    CHECK(12, oldu == 0 && is[0] == 0xFFFF);
    c = 0xFF;
    c++;
    u = 0x01FF;
    u += 1;
    l = 0;
    l -= 1;
    CHECK(13, c == 0 && u == 0x0200 && l == -1l);
    pass();
    return 0;
}
