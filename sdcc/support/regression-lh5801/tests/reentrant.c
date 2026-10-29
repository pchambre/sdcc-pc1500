/* Reentrant functions (src/lh5801/gen.c's collectFrameSave()): their
   locals are static, saved on the stack at entry and restored before
   they return -- direct and mutual recursion, a long result, an array
   local, a value kept across the recursive call, and a callback that
   comes back in through a pointer. A local a pointer can reach has a
   stack slot too (ralloc.c's planSlots()): one the callee writes through
   a pointer, an array and a struct the deeper levels read, and a call's
   result assigned to the local whose address it was given. */
#include "check.h"

volatile int vn = 10;
volatile unsigned char v6 = 6;

static int fib(int n) __reentrant
{
    if (n < 2)
        return n;
    return fib(n - 1) + fib(n - 2);
}

static long fact(unsigned char n) __reentrant
{
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

static unsigned char odd(unsigned char n) __reentrant;

static unsigned char even(unsigned char n) __reentrant
{
    return n == 0 ? 1 : odd(n - 1);
}

static unsigned char odd(unsigned char n) __reentrant
{
    return n == 0 ? 0 : even(n - 1);
}

/* the digits of n, written by each level into its own array, read back
   after the deeper levels have run */
static char out[8];
static unsigned char outLen;

static void digits(unsigned int n) __reentrant
{
    char d[2];
    volatile unsigned int keep = n;

    d[0] = '0' + n % 10;
    d[1] = 0;
    if (n >= 10)
        digits(n / 10);
    out[outLen++] = d[0];
    out[outLen] = 0;
    if (keep != n)
        out[0] = '?';
}

/* walks a tree through a callback that calls the walker again */
static unsigned char depth;
static unsigned char maxDepth;

static void walk(unsigned char n, void (*visit)(unsigned char)) __reentrant
{
    unsigned char mine = n;

    depth++;
    if (depth > maxDepth)
        maxDepth = depth;
    if (n)
        visit(n - 1);
    depth--;
    if (mine != n)
        maxDepth = 99;
}

static void visitor(unsigned char n)
{
    walk(n, visitor);
}

/* each level adds its total into its caller's x, through the pointer */
static void addTo(unsigned char n, int *acc) __reentrant
{
    int x = n;

    if (n)
        addTo(n - 1, &x);
    *acc += x;
}

/* each level's array holds a running sum the next one reads */
static unsigned int chainSum(unsigned char n, const unsigned char *up) __reentrant
{
    unsigned char buf[2];

    buf[0] = n;
    buf[1] = up ? up[0] + up[1] : 0;
    if (n)
        return chainSum(n - 1, buf);
    return buf[0] + buf[1];
}

struct pt
{
    int a;
    char b;
};

static int structDepth(unsigned char n, const struct pt *parent) __reentrant
{
    struct pt me;

    me.a = n;
    me.b = parent ? parent->b + 1 : 0;
    if (n)
        return structDepth(n - 1, &me);
    return me.a + me.b * 10 + (parent ? parent->a : 0) * 100;
}

static int bump(int *p) __reentrant
{
    int r = *p + 1;

    *p = 100;
    return r;
}

/* the call's result wins over the callee's write through &x; then a
   write through a pointer of its own */
static int selfAssign(int n) __reentrant
{
    int x = n;
    int *p = &x;

    x = bump(&x);
    if (x != n + 1)
        return -1;
    *p += 10;
    return x;
}

int main(void)
{
    CHECK(1, fib(vn) == 55);
    CHECK(2, fact(v6) == 720);
    CHECK(3, fact(12) == 479001600L);
    CHECK(4, even(v6) == 1 && odd(v6) == 0);
    CHECK(5, even(7) == 0 && odd(9) == 1);
    digits(40312);
    CHECK(6, outLen == 5);
    CHECK(7, out[0] == '4' && out[1] == '0' && out[2] == '3' && out[3] == '1' && out[4] == '2');
    walk(v6, visitor);
    CHECK(8, maxDepth == 7 && depth == 0);
    {
        int total = 0;

        addTo(4, &total);
        CHECK(9, total == 10);
    }
    CHECK(10, chainSum(5, 0) == 15);
    CHECK(11, structDepth(3, 0) == 130);
    CHECK(12, selfAssign(vn) == 21);
    pass();
    return 0;
}
