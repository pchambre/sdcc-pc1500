/* Control flow: loops, break/continue, goto, ?:, short-circuit && and ||
   with side effects, !, and switch (sparse and dense). */
#include "check.h"

volatile unsigned char v3 = 3, v0 = 0;
unsigned char hits;

unsigned char touch(unsigned char r)
{
    hits++;
    return r;
}

unsigned char sparse(unsigned char k)
{
    switch (k) {
    case 'H': return 1;
    case 'S': return 2;
    case 13: return 3;
    case '*': return 4;
    default: return 0;
    }
}

unsigned char dense(unsigned char k)
{
    switch (k) {
    case 0: return 10;
    case 1: return 11;
    case 2: return 12;
    case 3: return 13;
    case 4: return 14;
    case 5: return 15;
    case 6: return 16;
    case 7: return 17;
    default: return 99;
    }
}

int main(void)
{
    unsigned char i, n;
    int total;

    total = 0;
    for (i = 0; i < 20; i++) {
        if (i == 15)
            break;
        if (i & 1)
            continue;
        total += i;
    }
    CHECK(1, total == 56);                  /* 0+2+...+14 */
    n = 0;
    do
        n += 3;
    while (n < 10);
    CHECK(2, n == 12);
    i = 5;
    while (i--)
        n++;
    CHECK(3, n == 17 && i == 255);
    i = 0;
again:
    if (++i < 4)
        goto again;
    CHECK(4, i == 4);
    CHECK(5, (v3 > 2 ? 'Y' : 'N') == 'Y' && (v0 ? 1 : 2) == 2);
    hits = 0;
    CHECK(6, !(v0 && touch(1)) && hits == 0);
    CHECK(7, (v3 || touch(1)) && hits == 0);
    CHECK(8, (touch(0) || touch(1)) && hits == 2);
    CHECK(9, !v0 && !!v3);
    CHECK(10, sparse('H') == 1 && sparse(13) == 3 && sparse('*') == 4 && sparse('Q') == 0);
    CHECK(11, dense(0) == 10 && dense(v3) == 13 && dense(7) == 17 && dense(8) == 99 && dense(200) == 99);
    pass();
    return 0;
}
