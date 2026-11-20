/*-------------------------------------------------------------------------
   vocab.c - Colossal Cave Adventure for the PC-1500: the vocabulary
   (jwodder's vocab() from util.c), in the bank with the word table.

   gen.py's table: per word, its first five letters (NUL-padded) and the
   original's two values, 2 bytes each; sorted, as the original's.
-------------------------------------------------------------------------*/

#ifndef HOST
#pragma bank 0
#endif
#include "advent.h"

extern const unsigned char vocab_data[];

#define ENTRY 9

static int cmp5 (const char *w, const unsigned char *e)
{
    unsigned char i;

    for (i = 0; i < 5; i++) {
        if ((unsigned char) w[i] != e[i])
            return (unsigned char) w[i] < e[i] ? -1 : 1;
        if (w[i] == 0)
            return 0;
    }
    return 0;
}

/* A word's value: with type -1 its first, else the one of that type
   (value / 1000); -1 for a word not there. */
int vocab (const char *word, int type) BANKED
{
    int low = 0, high = WORDQTY - 1, i = 0, v1, v2;
    const unsigned char *e = vocab_data;

    while (low <= high) {
        int c;

        i = (low + high) / 2;
        e = vocab_data + i * ENTRY;
        c = cmp5 (word, e);
        if (c < 0)
            high = i - 1;
        else if (c > 0)
            low = i + 1;
        else
            break;
    }
    if (low > high) {
        if (type >= 0)
            bug (5);
        return -1;
    }
    v1 = (e[5] << 8) | e[6];
    v2 = (e[7] << 8) | e[8];
    return type >= 0 ? (v1 / 1000 == type ? v1 : v2) % 1000 : v1;
}
