/* In-place updates of array elements and struct members at constant
   addresses and through pointers or variable indexes (gen.c's
   genMemUpdate()/genMemUpdatePtr()), and of plain variables
   (genAddSubInPlace()): carries and borrows across bytes, wrap-around,
   masks, a constant the ADI chain can't take, a pointer still needed
   after the update, and updates whose old value is used, which must not
   be fused. */
#include "check.h"

unsigned char cs[4];
unsigned int is[4];
long ls[3];
struct {
    unsigned char a;
    int b;
} st;
struct sb {
    unsigned char a;
    int b;
};
struct sb sbs[2];
unsigned char c;
unsigned int u;
long l;
volatile unsigned char v0 = 0, v1 = 1, v2 = 2;

/* the pointer is still needed after each update but the last (each
   updates a different element: the same one twice reuses the value) */
static unsigned int twice(unsigned int *p)
{
    p[0] += 1;
    p[1] -= 0x0200;                         /* not an ADI chain */
    return p[1];
}

static void lmask(long *p)
{
    p[0] += 1;
    p[1] &= 0x7FFF00FFl;
    p[2] |= 0x00008000l;
}

static void member(struct sb *p)
{
    p->b -= 1;
    p->a |= 0x11;
}

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

    /* through a variable index or a pointer */
    cs[v2] = 254;
    cs[v2] += 3;
    CHECK(14, cs[2] == 1 && cs[1] == 0 && cs[3] == 0xC0);
    is[v1] = 0x00FF;
    is[v1] += 1;
    CHECK(15, is[1] == 0x0100 && is[0] == 0xFFFF && is[2] == 0x0122);
    is[v1] -= 0x0102;
    CHECK(16, is[1] == 0xFFFE);             /* borrow through the high byte */
    is[v1] += 0x0123;
    CHECK(17, is[1] == 0x0121);
    ls[v1] = 0x00FFFFFEl;
    ls[v1] += 5;
    CHECK(18, ls[1] == 0x01000003l && ls[0] == 0 && ls[2] == 0);
    ls[v1] -= 4;
    CHECK(19, ls[1] == 0x00FFFFFFl);
    ls[v2] += 0x00010000l;                  /* not an ADI chain */
    CHECK(20, ls[2] == 0x00010000l && ls[1] == 0x00FFFFFFl);
    is[v0] = 0x1234;
    is[v0] &= 0xFF0F;
    is[v0] |= 0x8000;
    CHECK(21, is[0] == 0x9204 && is[1] == 0x0121);
    is[3] = 0x02FF;
    CHECK(22, twice(&is[2]) == 0x00FF && is[2] == 0x0123 && is[1] == 0x0121);
    ls[0] = 0x1234FFFFl;
    lmask(&ls[0]);
    CHECK(23, ls[0] == 0x12350000l && ls[1] == 0x00FF00FFl && ls[2] == 0x00018000l);
    sbs[1].a = 0x80;
    sbs[1].b = 0x0100;
    member(&sbs[v1]);
    CHECK(24, sbs[1].a == 0x91 && sbs[1].b == 0x00FF && sbs[0].a == 0 && sbs[0].b == 0);
    for (c = 0; c < 4; c++)
        cs[c] += c;
    CHECK(25, cs[0] == 1 && cs[1] == 1 && cs[2] == 3 && cs[3] == 0xC3);
    pass();
    return 0;
}
