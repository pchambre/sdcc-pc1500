/* The ROM-number wrappers (pc1500.h, device/lib/lh5801/pc1500_*.asm):
   arithmetic, a few functions and STR$, on the real ROM's own routines.
   Numbers are BASIC's 8 bytes: exponent, sign, 5 bytes of BCD mantissa
   (d.ddddddddd), 0. */
#include "check.h"
#include <string.h>

static const pc1500_float_t three = {0x00, 0x00, 0x30, 0, 0, 0, 0, 0};
static const pc1500_float_t four = {0x00, 0x00, 0x40, 0, 0, 0, 0, 0};
static const pc1500_float_t n1500 = {0x03, 0x00, 0x15, 0, 0, 0, 0, 0};

static pc1500_float_t v;
static char s[24];

/* does s contain w? */
static unsigned char has(const char *s, const char *w)
{
    unsigned char i, j;

    for (i = 0; s[i]; i++) {
        for (j = 0; w[j] && s[i + j] == w[j]; j++)
            ;
        if (!w[j])
            return 1;
    }
    return 0;
}

static unsigned char is(const pc1500_float_t a, unsigned char e, unsigned char sign, unsigned char m)
{
    return a[0] == e && a[1] == sign && a[2] == m && a[3] == 0 && a[4] == 0 && a[5] == 0 &&
           a[6] == 0;
}

int main(void)
{
    memcpy(v, three, 8);
    pc1500Add(v, four);
    CHECK(1, is(v, 0x00, 0x00, 0x70));            /* 7 */
    CHECK(2, pc1500Error() == 0);
    memcpy(v, three, 8);
    pc1500Sub(v, four);
    CHECK(3, is(v, 0x00, 0x80, 0x10));            /* -1 */
    memcpy(v, three, 8);
    pc1500Mul(v, four);
    CHECK(4, is(v, 0x01, 0x00, 0x12));            /* 12 */
    memcpy(v, three, 8);
    pc1500Div(v, four);
    CHECK(5, is(v, 0xFF, 0x00, 0x75));           /* 0.75 */
    memcpy(v, four, 8);
    pc1500Sqrt(v);
    CHECK(6, is(v, 0x00, 0x00, 0x20));            /* 2 */
    memcpy(v, three, 8);
    pc1500Sub(v, four);
    pc1500Abs(v);
    CHECK(7, is(v, 0x00, 0x00, 0x10));            /* 1 */
    pc1500FloatToStr(n1500, s);
    CHECK(8, has(s, "1500") && strlen(s) <= 5);
    pc1500FloatToStr(v, s);
    CHECK(9, has(s, "1") && strlen(s) <= 2);
    pass();
    return 0;
}
