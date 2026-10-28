/* Reentrant functions (src/lh5801/gen.c's collectFrameSave()): their
   locals are static, saved on the stack at entry and restored before
   they return -- direct and mutual recursion, a long result, an array
   local, a value kept across the recursive call, and a callback that
   comes back in through a pointer. */
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
    pass();
    return 0;
}
