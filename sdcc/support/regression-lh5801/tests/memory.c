/* Arrays, 2-D arrays, pointers and pointer arithmetic, structs, strings,
   and pointers to functions. */
#include <string.h>
#include "check.h"

struct card {
    char rank;
    unsigned char value;
    int count;
};

struct card cards[4];
char rows[3][16];
volatile unsigned char vi = 2;
volatile unsigned int vz = 0;
int table[10];
unsigned char big[512];
unsigned char copy[320];

/* Called through a pointer with arguments, so SDCC wants them reentrant
   (parameters on the stack). */
unsigned char twice(unsigned char v) __reentrant { return v + v; }
unsigned char thrice(unsigned char v) __reentrant { return v + v + v; }

int main(void)
{
    unsigned char i, j;
    char *p;
    struct card *c;
    unsigned char (*fn)(unsigned char) __reentrant;
    int sum;

    for (i = 0; i < 10; i++)
        table[i] = i * 100;
    CHECK(1, table[0] == 0 && table[9] == 900 && table[vi] == 200);
    sum = 0;
    for (i = 0; i < 10; i++)
        sum += table[i];
    CHECK(2, sum == 4500);

    cards[1].rank = 'K';
    cards[1].value = 10;
    cards[1].count = -1;
    cards[vi].rank = 'A';
    cards[vi].count = 1000;
    c = &cards[1];
    CHECK(3, c->rank == 'K' && c->value == 10 && c->count == -1);
    c++;
    CHECK(4, c->rank == 'A' && c->count == 1000);
    c->count += 5;
    CHECK(5, cards[2].count == 1005);

    for (i = 0; i < 3; i++)
        for (j = 0; j < 15; j++)
            rows[i][j] = 'A' + i + j;
    CHECK(6, rows[0][0] == 'A' && rows[2][14] == 'A' + 16 && rows[vi][1] == 'D');

    strcpy(rows[1], "HELLO");
    strcat(rows[1], ", PC");
    CHECK(7, strlen(rows[1]) == 9 && !strcmp(rows[1], "HELLO, PC"));
    CHECK(8, strcmp(rows[1], "HELLO") > 0 && strcmp("ABC", "ABD") < 0);
    p = rows[1];
    while (*p != ',')
        p++;
    CHECK(9, p - rows[1] == 5 && p[2] == 'P');
    memset(rows[0], '*', 4);
    CHECK(10, rows[0][3] == '*' && rows[0][4] == 'E');
    memcpy(rows[2], rows[1], 6);
    CHECK(11, rows[2][0] == 'H' && rows[2][5] == ',');

    fn = vi == 2 ? twice : thrice;
    CHECK(12, fn(21) == 42);
    fn = thrice;
    CHECK(13, fn(vi) == 6);

    *(volatile unsigned char *)&table[0] = 0x12;
    CHECK(14, table[0] == 0x1200);          /* big-endian: byte 0 is the high one */

    /* the TIN/SIN/LIN/CIN string routines (device/lib/lh5801/string_*.asm) */
    for (i = 0; i < 255; i++) {
        big[i] = i;
        big[i + 255] = 255 - i;
    }
    CHECK(15, memcpy(copy, big, 300) == copy && copy[0] == 0 && copy[254] == 254 &&
              copy[255] == 255 && copy[299] == 211 && copy[300] == 0);
    CHECK(16, memcpy(copy, "XY", vz) == copy && copy[0] == 0);   /* n = 0 copies nothing */
    CHECK(17, memset(copy, 0x1A5, 260) == copy && copy[0] == 0xA5 && copy[259] == 0xA5 &&
              copy[260] == 250);            /* only c's low byte, exactly 260 (copy[260] is still big[260]) */
    CHECK(18, memset(copy, 'Q', vz) == copy && copy[0] == 0xA5);
    CHECK(19, strcpy(rows[0], "") == rows[0] && rows[0][0] == 0 && strlen(rows[0]) == 0);
    CHECK(20, strcpy(rows[0], "TIN") == rows[0] && strlen(rows[0]) == 3 && rows[0][3] == 0);
    CHECK(21, strlen(big + 1) == 509);      /* big[1..509] are 1..255..1, big[510] is 0: a 16-bit length */
    pass();
    return 0;
}
