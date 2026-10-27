/* Overlaid locals (src/lh5801/overlay.c): a function's locals must
   survive every call it makes -- down a chain, to a callee evaluated as
   another call's argument, through a function pointer (taken by name or
   only from an initializer), and around a recursive function -- while
   functions that never nest share memory. The arrays keep locals in
   memory rather than in registers. */
#include "check.h"

volatile unsigned char v1 = 1, v3 = 3;

static unsigned char fill(unsigned char *p, unsigned char base)
{
    unsigned char i;

    for (i = 0; i < 4; i++)
        p[i] = base + i;
    return base;
}

static unsigned char sum(const unsigned char *p)
{
    return p[0] + p[1] + p[2] + p[3];
}

/* a leaf whose locals overlay any caller's that it doesn't nest in */
static unsigned char leaf(unsigned char x)
{
    unsigned char t[4];

    fill(t, x);
    return sum(t);                          /* 4x + 6 */
}

static unsigned char mid(unsigned char x)
{
    unsigned char t[4];
    unsigned char r;

    fill(t, x + 10);
    r = leaf(x);
    return r + sum(t);                      /* 4x+6 + 4x+46 */
}

static unsigned char top(unsigned char x)
{
    unsigned char t[4];
    unsigned char r;

    fill(t, x + 20);
    r = mid(x);
    return r + sum(t);                      /* 8x+52 + 4x+86 */
}

static unsigned char pair(unsigned char a, unsigned char b)
{
    unsigned char t[4];

    fill(t, a);
    return leaf(b) + sum(t);
}

/* pointer-called functions take no arguments unless __reentrant, so the
   callbacks read theirs from cbArg */
static unsigned char cbArg;

static unsigned char viaPtr(unsigned char (*f)(void), unsigned char x)
{
    unsigned char t[4];
    unsigned char r;

    fill(t, x + 30);
    cbArg = x;
    r = f();
    return r + sum(t);
}

static unsigned char cbLeaf(void)
{
    unsigned char t[4];

    fill(t, cbArg + 40);
    return sum(t);                          /* 4x + 166 */
}

static unsigned char cbViaLeaf(void)
{
    return leaf(cbArg);
}

/* only named in an initializer */
static unsigned char cbTable(void)
{
    unsigned char t[4];

    fill(t, cbArg + 60);
    return sum(t);                          /* 4x + 246 */
}

static unsigned char (*const table[])(void) = { cbTable };

/* recursive, so its locals and parameter are its own -- and, not being
   reentrant, it is done with them before it recurses */
static unsigned char acc;

static void depth(unsigned char n)
{
    unsigned char t[4];

    if (n == 0)
        return;
    fill(t, n);
    leaf(n + 1);
    acc += sum(t);                          /* 4n + 6 */
    depth(n - 1);
}

int main(void)
{
    unsigned char t[4];

    fill(t, 50);
    CHECK(1, leaf(v1) == 10);
    CHECK(2, mid(v1) == 10 + 50);
    CHECK(3, top(v1) == 60 + 90);
    CHECK(4, pair(v1, v3) == 18 + 10);
    CHECK(5, pair(leaf(v1), mid(v3)) == (unsigned char)(4 * 76 + 6) + 46);
    CHECK(6, viaPtr(cbLeaf, v1) == (unsigned char)(170 + 130));
    CHECK(7, viaPtr(cbViaLeaf, v3) == 18 + 138);
    depth(v3);
    CHECK(8, acc == 18 + 14 + 10);
    CHECK(9, viaPtr(table[0], v1) == (unsigned char)(250 + 130));
    CHECK(10, sum(t) == 206);
    pass();
    return 0;
}
