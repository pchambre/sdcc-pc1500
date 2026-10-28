/* Values kept in Y or U across calls (gen.c's recordPreservedRegs()): a
   call to a function of the module that leaves them alone -- itself or
   through what it calls -- doesn't end their life, a call to one that
   uses them (directly, through a pointer copy, or by calling the
   library) does. Loops keep a pointer, a counter and a sum live across
   each call. */
#include "check.h"
#include <string.h>

volatile unsigned char seed = 3;
unsigned char data[6] = {1, 2, 3, 4, 5, 6};
char dst[8];

/* leaves Y and U alone */
static unsigned char twice(unsigned char a)
{
    return a + a;
}

/* so does this, through twice() */
static unsigned char quad(unsigned char a)
{
    return twice(twice(a));
}

/* uses Y and U: copies through two pointers */
static unsigned char copy2(char *d, const char *s)
{
    unsigned char n = 0;

    while ((*d++ = *s++))
        n++;
    return n;
}

/* calls the library, which may use anything */
static unsigned char lib(unsigned char a)
{
    return strlen("abc") + a;
}

static unsigned int sumTwice(const unsigned char *p, unsigned char n)
{
    unsigned int s = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        s += twice(p[i]);
    return s;
}

static unsigned int sumQuad(const unsigned char *p, unsigned char n)
{
    unsigned int s = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        s += quad(p[i]) + i;
    return s;
}

static unsigned int sumCopy(const unsigned char *p, unsigned char n)
{
    unsigned int s = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        s += copy2(dst, "xy") + p[i];
    return s;
}

static unsigned int sumLib(const unsigned char *p, unsigned char n)
{
    unsigned int s = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        s += lib(p[i]);
    return s;
}

int main(void)
{
    CHECK(1, sumTwice(data, 6) == 42);
    CHECK(2, sumQuad(data, 6) == 84 + 15);
    CHECK(3, sumCopy(data, 6) == 12 + 21);
    CHECK(4, sumLib(data, 6) == 18 + 21);
    CHECK(5, twice(seed) == 6 && quad(seed) == 12);
    CHECK(6, dst[0] == 'x' && dst[2] == 0);
    pass();
    return 0;
}
