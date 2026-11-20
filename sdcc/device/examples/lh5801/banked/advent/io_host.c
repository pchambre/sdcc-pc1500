/*-------------------------------------------------------------------------
   io_host.c - io.c for a PC (host.sh): plain text on stdout, commands
   from stdin, and the saved game in advent.sav. With "-S seed" the
   generator starts at seed, as jwodder's original does with -S, so the
   two can be run on the same commands and compared.
-------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "advent.h"

#define SAVE_FILE "advent.sav"

bool savable;

static unsigned long seed = 1;
static bool resumeOk;
static bool midLine;

void hostInit (int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp (argv[i], "-S") == 0 && i + 1 < argc)
            seed = strtoul (argv[++i], NULL, 10);
        else if (strcmp (argv[i], "-r") == 0)
            resumeOk = true;    /* offer advent.sav, if there */
    }
}

void startSeed (void)
{
    seedRan (seed);
}

void outc (char c)
{
    putchar (c);
    midLine = c != '\n';
}

void outs (const char *s)
{
    while (*s)
        outc (*s++);
}

void outn (int n)
{
    char b[8];

    sprintf (b, "%d", n);
    outs (b);
}

void endpara (void)
{
    if (midLine)
        outc ('\n');
}

void sayc (unsigned int n)
{
    TSAY_DISPATCH (n);
}

void sayb (unsigned int n)
{
    endpara ();
    sayc (n);
}

void say (unsigned int n)
{
    sayb (n);
    endpara ();
}

void rspeak (int msg)
{
    if (msg != 0)
        say (T_RMSG + msg);
}

void pspeak (int item, int state)
{
    say (T_ITEM + item * 7 + state + 1);
}

void finish (void)
{
    endpara ();
    exit (0);
}

static char upper (char c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

void getin (char *w1, char *r1, char *w2, char *r2)
{
    char line[128], *start, *end, *nl;
    unsigned char i;

    endpara ();
    for (;;) {
        fputs ("> ", stdout);
        if (fgets (line, sizeof line, stdin) == NULL)
            finish ();
        if ((nl = strchr (line, '\n')) != NULL)
            *nl = 0;
        line[INLEN] = 0;
        for (start = line; *start == ' '; start++)
            ;
        if (*start)
            break;
        if (!blklin) {
            *w1 = 0;
            if (r1 != NULL)
                *r1 = 0;
            if (w2 != NULL)
                *w2 = 0;
            if (r2 != NULL)
                *r2 = 0;
            return;
        }
    }
    for (end = start; *end && *end != ' '; end++)
        ;
    for (i = 0; i < 5 && start + i < end; i++)
        w1[i] = upper (start[i]);
    w1[i] = 0;
    if (r1 != NULL) {
        i = end - start;
        memcpy (r1, start, i);
        r1[i] = 0;
    }
    if (w2 != NULL || r2 != NULL) {
        for (start = end; *start == ' '; start++)
            ;
        for (end = start; *end && *end != ' '; end++)
            ;
        if (w2 != NULL) {
            for (i = 0; i < 5 && start + i < end; i++)
                w2[i] = upper (start[i]);
            w2[i] = 0;
        }
        if (r2 != NULL) {
            i = end - start;
            memcpy (r2, start, i);
            r2[i] = 0;
        }
    }
}

bool yes (int x, int y, int z)
{
    char reply[6];

    for (;;) {
        rspeak (x);
        getin (reply, NULL, NULL, NULL);
        if (strcmp (reply, "YES") == 0 || strcmp (reply, "Y") == 0) {
            rspeak (y);
            return true;
        } else if (strcmp (reply, "NO") == 0 || strcmp (reply, "N") == 0) {
            rspeak (z);
            return false;
        } else
            say (M_ANSWER);
    }
}

void saveGame (void)
{
    FILE *f = fopen (SAVE_FILE, "wb");

    if (f != NULL) {
        fwrite (&game, sizeof game, 1, f);
        fclose (f);
    }
}

bool loadGame (void)
{
    FILE *f = fopen (SAVE_FILE, "rb");
    bool ok;

    if (f == NULL)
        return false;
    ok = fread (&game, sizeof game, 1, f) == 1;
    fclose (f);
    return ok;
}

bool haveSave (void)
{
    FILE *f;

    if (!resumeOk || (f = fopen (SAVE_FILE, "rb")) == NULL)
        return false;
    fclose (f);
    return true;
}
