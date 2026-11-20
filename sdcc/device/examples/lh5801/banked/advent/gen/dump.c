/* dump.c - a host program that prints the original game's tables
   (orig/advtext.c, travel.c, vocab.c, miscdata.c) as JSON for gen.py.
   Built and run by gen.sh; never part of the PC-1500 program. */

#include <stdio.h>
#include "../orig/advdecl.h"

/* advdecl.h declares these as extern; dump.c doesn't use the game state. */
struct advgame game;

static void str (const char *s)
{
    if (s == NULL) {
        fputs ("null", stdout);
        return;
    }
    putchar ('"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            printf ("\\%c", *s);
        else if ((unsigned char) *s < 0x20)
            printf ("\\u%04x", (unsigned char) *s);
        else
            putchar (*s);
    }
    putchar ('"');
}

static void strs (const char *name, const char *const *a, int n)
{
    printf ("\"%s\": [", name);
    for (int i = 0; i < n; i++) {
        if (i)
            putchar (',');
        str (a[i]);
    }
    puts ("],");
}

static void ints (const char *name, const int *a, int n)
{
    printf ("\"%s\": [", name);
    for (int i = 0; i < n; i++)
        printf ("%s%d", i ? "," : "", a[i]);
    puts ("],");
}

int main (void)
{
    puts ("{");
    strs ("rmsg", rmsg, 202);
    strs ("longdesc", longdesc, 141);
    strs ("shortdesc", shortdesc, 141);
    fputs ("\"itemDesc\": [", stdout);
    for (int i = 0; i < 65; i++) {
        fputs (i ? ",[" : "[", stdout);
        for (int j = 0; j < 7; j++) {
            if (j)
                putchar (',');
            str (itemDesc[i][j]);
        }
        putchar (']');
    }
    puts ("],");
    fputs ("\"travel\": [", stdout);
    for (int l = 0; l < 141; l++) {
        fputs (l ? ",[" : "[", stdout);
        for (int r = 0; r < MAXROUTE && travel[l][r][0] != -1; r++) {
            fputs (r ? ",[" : "[", stdout);
            for (int k = 0; k < MAXTRAV && travel[l][r][k] != -1; k++)
                printf ("%s%d", k ? "," : "", travel[l][r][k]);
            putchar (']');
        }
        putchar (']');
    }
    puts ("],");
    fputs ("\"vocabulary\": [", stdout);
    for (int i = 0; i < WORDQTY; i++) {
        fputs (i ? ",[" : "[", stdout);
        str (vocabulary[i].word);
        printf (",%d,%d]", vocabulary[i].val1, vocabulary[i].val2);
    }
    puts ("],");
    ints ("actspk", actspk, 32);
    ints ("cond", cond, 141);
    fputs ("\"classes\": [", stdout);
    for (int i = 0; classes[i].score != 0; i++) {
        printf ("%s[%d,", i ? "," : "", classes[i].score);
        str (classes[i].rank);
        putchar (']');
    }
    puts ("],");
    fputs ("\"hints\": [", stdout);
    for (int i = 0; i < 10; i++)
        printf ("%s[%d,%d,%d,%d]", i ? "," : "", hints[i][0], hints[i][1],
                hints[i][2], hints[i][3]);
    puts ("]");
    puts ("}");
    return 0;
}
