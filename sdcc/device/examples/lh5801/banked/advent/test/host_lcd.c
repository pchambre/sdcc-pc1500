/*-------------------------------------------------------------------------
   host_lcd.c - the PC-1500 as io.c sees it, on a PC, for lcdtest.sh:
   a 26-character display, and a player who presses ENTER at every line
   and types the next command from a file at every prompt. Each time
   the program waits for a key, the display and the key pressed are
   printed, one line each: "<display>\t<key>". The game and io.c are the
   PC-1500's own code, so the emulated PC-1500, given the same keys and
   seed, has to show the same displays (lcdtest.sh checks that).

   Usage: advent-lcd -S seed commands-file
-------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pc1500.h>
#include "advent.h"

#define W 26

static char screen[W + 1];
static unsigned char cell;
static unsigned long seed = 1;
static FILE *cmds;
static char cmd[128];
static int pos = -1;        /* in cmd while typing it, else -1 */

unsigned char save_area[SAVE_SIZE];

void hostInit (int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp (argv[i], "-S") == 0 && i + 1 < argc)
            seed = strtoul (argv[++i], NULL, 10);
        else if ((cmds = fopen (argv[i], "r")) == NULL) {
            perror (argv[i]);
            exit (2);
        }
    }
    if (cmds == NULL) {
        fputs ("usage: advent-lcd -S seed commands-file\n", stderr);
        exit (2);
    }
    memset (screen, ' ', W);
}

void hostSeed (void)
{
    seedRan (seed);
}

void lcdClear (void)
{
    memset (screen, ' ', W);
    cell = 0;
}

void lcdPutChar (unsigned char c)
{
    if (cell < W)
        screen[cell++] = c;
}

void lcdEditBegin (void)   /* the Screen pane's: nothing on a PC */
{
}

void lcdEditEnd (void)
{
}

unsigned char keyScan (void)
{
    static int n;

    return n++ == 0;    /* the title's key: down, then up */
}

void idleTick (void)
{
}

static void shown (const char *key)
{
    int n = W;

    while (n > 0 && screen[n - 1] == ' ')
        n--;
    printf ("%.*s\t%s\n", n, screen, key);
}

unsigned char keyWait (void)
{
    char k[2] = {0, 0};

    if (strncmp (screen, "> ", 2) != 0) {
        shown ("ENT");
        return PC1500_KEY_ENTER;
    }
    if (pos < 0) {
        char *nl;

        if (fgets (cmd, sizeof cmd, cmds) == NULL) {
            shown ("END");
            exit (0);
        }
        if ((nl = strchr (cmd, '\n')) != NULL)
            *nl = 0;
        cmd[W - 3] = 0;         /* what fits the prompt */
        pos = 0;
    }
    if (cmd[pos] == 0) {
        pos = -1;
        shown ("ENT");
        return PC1500_KEY_ENTER;
    }
    k[0] = cmd[pos++];
    shown (k[0] == ' ' ? "SPACE" : k);
    return (unsigned char) k[0];
}

void bank_select (unsigned char bank)
{
    (void) bank;
}

unsigned char bank_current (void)
{
    return 0;
}
