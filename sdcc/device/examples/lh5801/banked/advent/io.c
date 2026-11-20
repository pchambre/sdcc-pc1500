/*-------------------------------------------------------------------------
   io.c - Colossal Cave Adventure for the PC-1500: the text on the
   26-character display, the keyboard, and the saved game in a bank
   (fixed memory).

   Text is word-wrapped into lines of 26. Each line stays on the display
   until ENTER (or the down arrow or SPACE); the up arrow goes back
   through the last HIST lines, there and at the "> " prompt. A command
   is typed at the prompt: CL clears it, ENTER sends it. BREAK saves the
   game in the card's RAM and goes back to BASIC; the next CALL offers to
   resume it.

   Fixed memory on the card, bank 4 on a CE-163F (see advent.h's
   BANKED_CE). The saved game's copying is in savegame.c.
-------------------------------------------------------------------------*/

#if defined ADV_CE163F && !defined HOST
#pragma bank 4          /* see advent.h's BANKED_CE */
#endif

#include <string.h>
#include <pc1500.h>
#include <pc1500_bank.h>
#include "advent.h"

#define W     26        /* the display's characters */
#define HIST  10        /* lines kept for the up arrow */
#define KEY_LEFT 0x08

bool savable;

#ifdef HOST    /* lcdtest.sh: io.c on a PC, host_lcd.c the PC-1500 */
static char cur[W], hist[HIST][W], line[W];
#else
#define cur ((char *) AT_CUR)        /* the line being filled */
#define hist ((char (*)[W]) AT_HIST) /* the lines shown, oldest overwritten */
#define line ((char *) AT_LINE)     /* the command line */
#endif
static unsigned char ncur;
static unsigned char hnext, hcount;
static bool waiting;                /* a line is shown that ENTER hasn't passed */

static void show (const char *l, unsigned char n)
{
    unsigned char i;

    lcdClear ();
    for (i = 0; i < n; i++)
        lcdPutChar (l[i]);
}

static const char *histLine (unsigned char back)
{
    return hist[(unsigned char) (hnext + HIST - 1 - back) % HIST];
}

static void breakOut (void)
{
    lcdEditEnd ();          /* BASIC's line ended for it (pc1500.h) */
    if (savable)
        saveGame ();
    lcdClear ();
    exit (0);
}

/* The up arrow pressed: back through the lines shown, the down arrow
   forward again, past the newest line to leave. */
static void browse (void)
{
    unsigned char back = 0, k = PC1500_KEY_UP;

    for (;;) {
        if (k == PC1500_KEY_UP) {
            if (back + 1 < hcount)
                back++;
        } else if (k == PC1500_KEY_DOWN || k == PC1500_KEY_ENTER) {
            if (back == 0)
                return;
            back--;
        } else if (k == PC1500_KEY_BREAK)
            breakOut ();
        show (histLine (back), W);
        k = keyWait ();
    }
}

static char ahead[8];               /* typed while a line waited */
static unsigned char nahead;

/* Wait for the player to pass the line on the display. A letter (any
   character but a space) passes it too, and begins the next command: it
   is kept for the prompt, and so are the spaces after it. */
static void more (void)
{
    unsigned char k;

    for (;;) {
        k = keyWait ();
        if (k == PC1500_KEY_ENTER || k == PC1500_KEY_DOWN || (k == ' ' && !nahead))
            break;
        if (k >= ' ' && k < 0x7F) {
            if (nahead < sizeof ahead)
                ahead[nahead++] = k;
            break;
        }
        if (k == PC1500_KEY_BREAK)
            breakOut ();
        if (k == PC1500_KEY_UP) {
            browse ();
            break;
        }
    }
    waiting = false;
}

static void addHist (const char *s, unsigned char n)
{
    char *h = hist[hnext];

    memcpy (h, s, n);
    memset (h + n, ' ', W - n);
    hnext = (unsigned char) (hnext + 1) % HIST;
    if (hcount < HIST)
        hcount++;
}

static void emit (const char *s, unsigned char n)
{
    if (waiting)
        more ();
    addHist (s, n);
    show (s, n);
    waiting = true;
}

void endpara (void) BANKED_CE
{
    if (ncur) {
        emit (cur, ncur);
        ncur = 0;
    }
}

void outc (char c) BANKED_CE
{
    unsigned char i;

    if (c == '\n') {
        endpara ();
        return;
    }
    if (c == ' ' && ncur == 0)
        return;
    if (ncur == W) {
        if (c == ' ') {
            endpara ();
            return;
        }
        for (i = W; i && cur[i - 1] != ' '; i--)
            ;
        if (i == 0)
            endpara ();         /* a word as long as the line */
        else {
            unsigned char j;

            emit (cur, i - 1);
            ncur = W - i;
            for (j = 0; j < ncur; j++)
                cur[j] = cur[i + j];
        }
    }
    cur[ncur++] = c;
}

void outs (const char *s) BANKED_CE
{
    while (*s)
        outc (*s++);
}

void outn (int n) BANKED_CE
{
    char b[7], *p = b + 6;

    *p = 0;
    if (n < 0) {
        outc ('-');
        n = -n;
    }
    do {
        *--p = '0' + n % 10;
        n /= 10;
    } while (n);
    outs (p);
}

void sayc (unsigned int n) BANKED_CE
{
    TSAY_DISPATCH (n);
}

void sayb (unsigned int n) BANKED_CE
{
    endpara ();
    sayc (n);
}

void say (unsigned int n) BANKED_CE
{
    sayb (n);
    endpara ();
}

void rspeak (int msg) BANKED_CE
{
    if (msg != 0)
        say (T_RMSG + msg);
}

void pspeak (int item, int state) BANKED_CE
{
    say (T_ITEM + item * 7 + state + 1);
}

/* The "> " prompt and a command, up to W - 3 characters (buf holds W);
   returns its length. */
static unsigned char readLine (char *buf)
{
    unsigned char n = 0, k;

    endpara ();
    if (waiting)
        more ();
    buf[0] = '>';
    buf[1] = ' ';
    buf += 2;
    memcpy (buf, ahead, nahead);
    n = nahead;
    nahead = 0;
    buf[n] = 0;
    lcdEditBegin ();        /* the line being typed, for the Screen pane */
    for (;;) {
        show (buf - 2, n + 2);
        for (;;) {
            k = keyWait ();
            if (k == PC1500_KEY_ENTER) {
                lcdEditEnd ();
                addHist (buf - 2, n + 2);   /* the command in the history too */
                return n;
            }
            if (k == PC1500_KEY_BREAK)
                breakOut ();
            if (k == PC1500_KEY_UP) {
                browse ();
                break;
            }
            if (k == PC1500_KEY_CL || (k == KEY_LEFT && n)) {
                if (k == PC1500_KEY_CL)
                    n = 0;
                else
                    n--;
                buf[n] = 0;
                break;
            }
            if (k >= ' ' && k < 0x7F && n < W - 3) {   /* keeps a NUL in buf */
                buf[n++] = k;
                buf[n] = 0;
                lcdPutChar (k);
            }
        }
    }
}

static bool isspc (char c)
{
    return c == ' ';
}

static char upper (char c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

/* jwodder's getin(): the first word in r1 and its first five letters,
   upper case, in w1; the same for the second word. A blank line is asked
   again while blklin is set. */
void getin (char *w1, char *r1, char *w2, char *r2) BANKED_CE
{
    char *start, *end;
    unsigned char i;

    for (;;) {
        readLine (line);
        for (start = line + 2; isspc (*start); start++)
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
    for (end = start; *end && !isspc (*end); end++)
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
        for (start = end; isspc (*start); start++)
            ;
        for (end = start; *end && !isspc (*end); end++)
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

bool yes (int x, int y, int z) BANKED_CE
{
    char reply[6];
    bool was = savable;

    savable = false;    /* BREAK in the middle of a question doesn't save */
    for (;;) {
        rspeak (x);
        getin (reply, NULL, NULL, NULL);
        if (strcmp (reply, "YES") == 0 || strcmp (reply, "Y") == 0) {
            rspeak (y);
            savable = was;
            return true;
        } else if (strcmp (reply, "NO") == 0 || strcmp (reply, "N") == 0) {
            rspeak (z);
            savable = was;
            return false;
        } else
            say (M_ANSWER);
    }
}

void finish (void) BANKED_CE
{
    endpara ();
    if (waiting)
        more ();
    lcdClear ();
    exit (0);
}

/* The title screen. The time until the first key seeds the generator, as
   the clock did the original's. */
void startSeed (void) BANKED_CE
{
    unsigned long n = 0;

    show ("COLOSSAL CAVE ADVENTURE", 23);
    while (!keyScan ()) {
        idleTick ();
        if (++n == 5UL * 60 * PC1500_TICKS_PER_SECOND) {
            keyWait ();     /* five minutes: the ROM's own wait, which powers off */
            break;
        }
    }
    while (keyScan ())
        n++;
    seedRan ((n * 18 + 5) & 0xFFFFFUL);
#ifdef HOST
    {
        extern void hostSeed (void);    /* test/host_lcd.c: -S */
        hostSeed ();
    }
#endif
}
