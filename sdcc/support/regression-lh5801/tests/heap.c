/* heap.c - malloc, calloc, realloc and free on the default 1K
   __sdcc_heap (device/lib/lh5801/heap.c), and sprintf into what they
   return: blocks don't overlap, keep their contents, come back after
   free, and an impossible request is NULL. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"

volatile unsigned int big = 4000; /* more than the heap holds */
volatile int value = -1234;

void main(void)
{
    unsigned char *a, *b, *c;
    char *s;
    unsigned int i;

    a = malloc(100);
    b = malloc(200);
    CHECK(1, a && b);
    CHECK(2, b >= a + 100 || a >= b + 200); /* no overlap */
    memset(a, 0xA5, 100);
    for (i = 0; i < 200; i++)
        b[i] = (unsigned char)i;
    for (i = 0; i < 100; i++)
        CHECK(3, a[i] == 0xA5);
    for (i = 0; i < 200; i++)
        CHECK(4, b[i] == (unsigned char)i);

    c = calloc(10, 8); /* zeroed */
    CHECK(5, c != NULL);
    for (i = 0; i < 80; i++)
        CHECK(6, c[i] == 0);

    a = realloc(a, 300); /* grows: the first 100 bytes kept */
    CHECK(7, a != NULL);
    for (i = 0; i < 100; i++)
        CHECK(8, a[i] == 0xA5);
    for (i = 0; i < 200; i++)
        CHECK(9, b[i] == (unsigned char)i);

    CHECK(10, malloc(big) == NULL);

    free(b);
    free(c);
    b = malloc(250); /* the freed space comes back */
    CHECK(11, b != NULL);
    free(b);
    free(a);
    a = malloc(900); /* all of it free again: one big block */
    CHECK(12, a != NULL);
    free(a);

    s = malloc(32);
    CHECK(13, s != NULL);
    sprintf(s, "%d %u %x %s", value, 65535u, 0xBEEF, "PC");
    CHECK(14, strcmp(s, "-1234 65535 beef PC") == 0);
    free(s);

    pass();
}
