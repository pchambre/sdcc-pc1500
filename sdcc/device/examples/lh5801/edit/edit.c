/* edit.c - a line editor for the Sharp PC-1500, written for the LH5801
   SDCC port as a third real program to measure the compiler against
   (with Blackjack and LANDER). Where those are games, this is the other
   kind of program people write: strings, pointers, memmove() and loops
   over a text buffer, and no float.

   The text is up to TEXT_MAX bytes of lines in RAM; it lasts while the
   program runs. The prompt shows the current line's number:
     3*
   and takes a command (ENTER on its own shows the next line):
     n           go to line n and show it
     I           insert lines before the current one, until an empty line
     A           append lines at the end, the same way
     D [k]       delete k lines (default 1) from the current one
     L           browse: DOWN/UP move, ENTER or CL back to the prompt
     F/text      find text, from the line after the current one, wrapping
     R/old/new   replace old by new everywhere; shows how many
     S           sort the lines (ASCII order)
     C           count lines, characters and free bytes
     Q           quit
   A line shows as its number and as much of it as fits the 26 cells.
   BREAK (ON) at any prompt returns to BASIC.

   Build (support/scripts/build-lh5801.sh), reserve the RAM with the NEW
   the build prints, load the .bin and CALL it:
     build-lh5801.sh edit.c 00C5      (28K of RAM from 0000H)

   Paul Chambre, 2026 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pc1500.h>

#define TEXT_MAX 4096
#define LINE_MAX 80             /* one line, without its '\n' */
#define CELLS 26

char text[TEXT_MAX];            /* the lines, each ending in '\n' */
unsigned int used;              /* bytes of text in use */
unsigned int lines;             /* how many */
unsigned int cur;               /* the current line, 1..lines (0: none) */
unsigned int changes;           /* R's last count, for the test script */

static char cmd[LINE_MAX + 1];
static char out[CELLS + 1];

/* where line n (1-based) starts; n = lines + 1 is the end of the text */
static char *lineAt(unsigned int n)
{
    char *p = text;
    char *end = text + used;

    /* (char *) first: SDCC, on every port, takes void * + 1 as + 0 */
    while (--n && p < end)
        p = (char *)memchr(p, '\n', end - p) + 1;
    return p;
}

static unsigned int lineLen(const char *p)
{
    return (const char *)memchr(p, '\n', text + used - p) - p;
}

static void show(const char *s)
{
    lcdClear();
    lcdPutString(s);
}

/* shows s until a key; BREAK ends the program */
static unsigned char tell(const char *s)
{
    unsigned char k;

    show(s);
    k = keyWait();
    if (k == PC1500_KEY_BREAK)
        exit(0);
    return k;
}

/* "n text", cut to the display */
static void showLine(unsigned int n)
{
    const char *p = lineAt(n);
    unsigned char len, i;

    len = (unsigned char)sprintf(out, "%u ", n);
    for (i = 0; len < CELLS && p[i] != '\n'; i++)
        out[len++] = p[i];
    out[len] = 0;
    show(out);
}

/* reads a line after `prompt` into cmd; returns its length */
static unsigned char input(const char *prompt)
{
    unsigned char n;

    show(prompt);
    n = lcdInputLine(cmd, CELLS - (unsigned char)strlen(prompt));
    if (n == PC1500_INPUT_BREAK)
        exit(0);
    return n;
}

/* makes room for `len` bytes at p (or removes -len bytes there) */
static unsigned char resize(char *p, int len)
{
    if (len > 0 && used + len > TEXT_MAX)
        return 0;
    memmove(p + len, p, text + used - p);
    used += len;
    return 1;
}

/* inserts cmd's n bytes as a line before line `at` */
static unsigned char insertLine(unsigned int at, unsigned char n)
{
    char *p = lineAt(at);

    if (!resize(p, n + 1))
        return 0;
    memcpy(p, cmd, n);
    p[n] = '\n';
    lines++;
    return 1;
}

static void insert(unsigned int at)
{
    for (;;) {
        unsigned char n;

        sprintf(out, "%u:", at);
        n = input(out);
        if (!n)
            break;
        if (!insertLine(at, n)) {
            tell("TEXT FULL");
            break;
        }
        at++;
    }
    cur = at > lines ? lines : at;
}

static void delete(unsigned int k)
{
    char *from, *to;

    if (!cur)
        return;
    if (k > lines - cur + 1)
        k = lines - cur + 1;
    from = lineAt(cur);
    to = lineAt(cur + k);
    resize(to, -(int)(to - from));
    lines -= k;
    if (cur > lines)
        cur = lines;
}

static void browse(void)
{
    for (;;) {
        unsigned char k;

        if (!cur)
            return;
        showLine(cur);
        k = keyWait();
        if (k == PC1500_KEY_BREAK)
            exit(0);
        if (k == PC1500_KEY_DOWN && cur < lines)
            cur++;
        else if (k == PC1500_KEY_UP && cur > 1)
            cur--;
        else if (k == PC1500_KEY_ENTER || k == PC1500_KEY_CL)
            return;
    }
}

/* does line p hold s (length n)? */
static unsigned char holds(const char *p, const char *s, unsigned char n)
{
    unsigned int len = lineLen(p);
    unsigned int i;

    for (i = 0; i + n <= len; i++)
        if (!memcmp(p + i, s, n))
            return 1;
    return 0;
}

static void find(const char *s)
{
    unsigned char n = (unsigned char)strlen(s);
    unsigned int i, at = cur;

    for (i = 0; i < lines; i++) {
        at = at % lines + 1;
        if (holds(lineAt(at), s, n)) {
            cur = at;
            showLine(cur);
            keyWait();
            return;
        }
    }
    tell("NOT FOUND");
}

/* R/old/new: every occurrence, left to right, never inside a
   replacement */
static void replace(char *arg)
{
    char *sep = strchr(arg, '/');
    unsigned char on, nn;
    char *p;

    changes = 0;
    if (!sep || sep == arg) {
        tell("R/OLD/NEW");
        return;
    }
    *sep++ = 0;
    on = (unsigned char)strlen(arg);
    nn = (unsigned char)strlen(sep);
    for (p = text; p + on <= text + used;) {
        if (*p != '\n' && !memcmp(p, arg, on) && !memchr(p, '\n', on)) {
            if (!resize(p + on, nn - on)) {
                tell("TEXT FULL");
                break;
            }
            memcpy(p, sep, nn);
            p += nn;
            changes++;
        } else
            p++;
    }
    sprintf(out, "%u CHANGES", changes);
    tell(out);
}

/* line a's text vs line b's, as strcmp */
static int compareLines(const char *a, const char *b)
{
    for (; *a == *b; a++, b++)
        if (*a == '\n')
            return 0;
    if (*a == '\n')
        return -1;
    if (*b == '\n')
        return 1;
    return (unsigned char)*a - (unsigned char)*b;
}

/* insertion sort, in place: each line moves in front of the first one
   it sorts before, by rotating the text between (no second buffer) */
static void sortLines(void)
{
    unsigned int i;

    for (i = 2; i <= lines; i++) {
        char *p = lineAt(i);
        unsigned int len = lineLen(p) + 1;
        unsigned int j;

        for (j = 1; j < i; j++) {
            char *q = lineAt(j);

            if (compareLines(p, q) < 0) {
                /* bring p's line in front of q, a byte at a time from its end */
                unsigned int k;

                for (k = 0; k < len; k++) {
                    char c = p[len - 1];

                    memmove(q + 1, q, p + len - 1 - q);
                    *q = c;
                }
                break;
            }
        }
    }
}

int main(void)
{
    for (;;) {
        unsigned char n;
        char *arg;

        sprintf(out, "%u*", cur);
        n = input(out);
        arg = cmd + 1;
        while (*arg == ' ')
            arg++;
        if (!n) {
            if (cur < lines)
                cur++;
            if (cur)
                showLine(cur);
            else
                show("NO TEXT");
            keyWait();
            continue;
        }
        if (cmd[0] >= '0' && cmd[0] <= '9') {
            unsigned int to = (unsigned int)atoi(cmd);

            if (to >= 1 && to <= lines) {
                cur = to;
                showLine(cur);
                keyWait();
            } else
                tell("NO SUCH LINE");
            continue;
        }
        switch (cmd[0]) {
        case 'I':
            insert(cur ? cur : 1);
            break;
        case 'A':
            insert(lines + 1);
            break;
        case 'D':
            delete(*arg ? (unsigned int)atoi(arg) : 1);
            break;
        case 'L':
            browse();
            break;
        case 'F':
            if (cmd[1] == '/' && cmd[2])
                find(cmd + 2);
            else
                tell("F/TEXT");
            break;
        case 'R':
            if (cmd[1] == '/')
                replace(cmd + 2);
            else
                tell("R/OLD/NEW");
            break;
        case 'S':
            sortLines();
            cur = lines ? 1 : 0;
            break;
        case 'C':
            sprintf(out, "%u LINES %u CH %u FREE", lines, used, TEXT_MAX - used);
            tell(out);
            break;
        case 'Q':
            return 0;
        default:
            tell("?  I A D L F R S C Q");
        }
    }
}
