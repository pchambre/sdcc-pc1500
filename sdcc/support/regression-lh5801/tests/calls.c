/* Calls: mixed parameter sizes, 1/2/4-byte returns, calls nested as
   arguments, a call chain, and varargs through sprintf(). */
#include <stdio.h>
#include <string.h>
#include "check.h"

volatile int vk = 7;
char out[32];

unsigned char mix(unsigned char a, int b, long c, unsigned char d)
{
    return (unsigned char)(a + b + c + d);
}

int scale(int v, unsigned char by)
{
    return v * by;
}

long widen(int v)
{
    return (long)v * 100000l;
}

unsigned char depth3(unsigned char v) { return v + 1; }
unsigned char depth2(unsigned char v) { return depth3(v) * 2; }
unsigned char depth1(unsigned char v) { return depth2(v) + depth3(v); }

int main(void)
{
    CHECK(1, mix(1, 300, 70000l, 4) == (unsigned char)(1 + 300 + 70000l + 4));
    CHECK(2, scale(vk, 9) == 63 && scale(-vk, 9) == -63);
    CHECK(3, widen(vk) == 700000l && widen(-3) == -300000l);
    CHECK(4, scale(scale(vk, 2), 3) == 42);
    CHECK(5, depth1(5) == 18);              /* (5+1)*2 + (5+1) */
    sprintf(out, "%d|%u|%ld|%s|%x", -vk, 65000u, -123456l, "PC", 0xBEEF);
    CHECK(6, !strcmp(out, "-7|65000|-123456|PC|beef"));
    pass();
    return 0;
}
