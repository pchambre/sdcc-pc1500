/* Startup: zeroed globals, initialized globals and arrays, static locals
   with initializers, const tables, and rand()'s own initialized state. */
#include <stdlib.h>
#include "check.h"

int zeroed[8];
long zlong;
int seven = 7;
long big = -123456789l;
char name[] = "BLACKJACK";
const char *const words[] = {"ONE", "TWO", "THREE"};
const unsigned char squares[] = {0, 1, 4, 9, 16, 25};

unsigned char counter(void)
{
    static unsigned char calls = 40;
    return ++calls;
}

int main(void)
{
    unsigned char i;
    int r1, r2;

    for (i = 0; i < 8; i++)
        CHECK(1, zeroed[i] == 0);
    CHECK(2, zlong == 0);
    CHECK(3, seven == 7 && big == -123456789l);
    CHECK(4, name[0] == 'B' && name[8] == 'K' && name[9] == 0);
    CHECK(5, words[2][0] == 'T' && words[2][4] == 'E' && words[0][1] == 'N');
    CHECK(6, squares[5] == 25);
    CHECK(7, counter() == 41 && counter() == 42);
    r1 = rand();
    r2 = rand();
    CHECK(8, r1 != r2 && r1 >= 0 && r2 >= 0);  /* rand()'s seed was initialized */
    pass();
    return 0;
}
