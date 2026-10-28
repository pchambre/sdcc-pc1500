/* Register parameters (src/lh5801/main.c): the first 1-byte parameter in
   A, the first 2-byte one in X, the rest in the callee's _PARM_n bytes --
   in either order, mixed with wider ones, as a call's argument to another
   call, kept by the callee across its own calls, and unused. */
#include "check.h"

volatile unsigned char vc = 5, vd = 9;
volatile unsigned int vi = 0x1234;
volatile long vl = 100000L;
char buf[8];

static unsigned char c1(unsigned char a)
{
    return a + 1;
}

static unsigned int i1(unsigned int x)
{
    return x + 1;
}

static unsigned int ci(unsigned char a, unsigned int x)
{
    return x + a;
}

static unsigned int ic(unsigned int x, unsigned char a)
{
    return x - a;
}

static unsigned char cc(unsigned char a, unsigned char b)
{
    return a - b;
}

static unsigned int ii(unsigned int x, unsigned int y)
{
    return x - y;
}

static long lc(long l, unsigned char a)
{
    return l + a;
}

/* the pointer in X must outlive the calls the function makes */
static unsigned char fill(char *p, unsigned char n)
{
    unsigned char i;

    for (i = 0; i < n; i++)
        p[i] = c1(i) + '0';
    p[n] = 0;
    return p[0] + p[n - 1];
}

/* both register parameters live across a call */
static unsigned int keep(unsigned char a, unsigned int x)
{
    unsigned int r = i1(x);

    return r + c1(a) + a + x;
}

static unsigned char unused(unsigned char a, unsigned int x)
{
    (void) a;
    (void) x;
    return 42;
}

int main(void)
{
    CHECK(1, c1(vc) == 6);
    CHECK(2, i1(vi) == 0x1235);
    CHECK(3, ci(vc, vi) == 0x1239);
    CHECK(4, ic(vi, vc) == 0x122F);
    CHECK(5, cc(vd, vc) == 4);
    CHECK(6, ii(vi, 0x34) == 0x1200);
    CHECK(7, lc(vl, vc) == 100005L);
    CHECK(8, ci(c1(vc), i1(vi)) == 0x123B);
    CHECK(9, ii(i1(vi), ic(vi, vc)) == 6);
    CHECK(10, fill(buf, 3) == '1' + '3');
    CHECK(11, buf[1] == '2' && buf[3] == 0);
    CHECK(12, keep(vc, vi) == 0x1235 + 6 + 5 + 0x1234);
    CHECK(13, unused(vc, vi) == 42);
    CHECK(14, cc(cc(vd, 1), cc(vc, 2)) == 5);
    pass();
    return 0;
}
