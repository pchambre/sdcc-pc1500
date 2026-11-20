/*-------------------------------------------------------------------------
   advent.h - Colossal Cave Adventure (Crowther & Woods, 350 points) for
   the PC-1500 with the internal expansion card's banked RAM.

   Ported from Jonathan Wodder's C translation of the original PDP-10
   Fortran (https://github.com/jwodder/advent350, commit a2f7ab0, "in
   the public domain"); its data files are in orig/. This port is in the
   public domain too.

   The game's logic is jwodder's, function for function; what changed is
   where it lives. Fixed memory (40C5H up) holds main() (start.c), the
   saved game's copying (savegame.c), every variable and, on the card,
   turn()'s dispatch and the small helpers everything calls (main.c) and
   the screen and keyboard code (io.c). The rest is in banks, which the
   compiler switches on each __banked call:

     bank 0  vocab.c (and the vocabulary), verbs.c, the saved game
     bank 1  text
     bank 2  text
     bank 3  text, turn.c
     bank 4  motion.c (and the travel table, which only it reads); on a
             PC-1500A with a CE-163F (ADV_CE163F), main.c and io.c too

   gen/gen.py makes the banks' data from orig/ (gen/gen.sh runs it).
   With HOST defined, the same files build for a PC (host.sh), to check
   the game against jwodder's original.
-------------------------------------------------------------------------*/

#ifndef ADVENT_H
#define ADVENT_H

#include <stdbool.h>

#ifdef HOST
#define BANKED
#define AT(a)
#else
#define BANKED __banked
#define AT(a) __at(a)
#endif

/* main.c and io.c: fixed memory on the card, where every bank switch
   wakes its MCU; bank 4 on a PC-1500A with a CE-163F (ADV_CE163F, make.sh
   --ce163f), where fixed memory is short and a switch is one store. */
#if defined ADV_CE163F && !defined HOST
#define BANKED_CE __banked
#else
#define BANKED_CE
#endif

/* io.c's buffers, kept in the system RAM of BASIC's fixed variables
   (A$-Z$ at 7650H-76FFH, A-Z at 78C0H-79CFH), which the game doesn't use:
   fixed memory is short. The startup code doesn't zero these. */
#define AT_CUR    0x7650    /* 26: the line being filled */
#define AT_LINE   0x766A    /* 26: the command line */
#define AT_HIST   0x78C0    /* 10 x 26: io.c's lines for the up arrow */

#include "advdata.h"

enum item {KEYS = 1, LAMP, GRATE, CAGE, ROD, ROD2, STEPS, BIRD, DOOR, PILLOW,
 SNAKE, FISSUR, TABLET, CLAM, OYSTER, MAGZIN, DWARF, KNIFE, FOOD, BOTTLE,
 WATER, OIL, MIRROR, PLANT, PLANT2, AXE = 28, DRAGON = 31, CHASM, TROLL,
 TROLL2, BEAR, MESSAG, VOLCANO, VEND, BATTER, NUGGET = 50, COINS = 54, CHEST,
 EGGS, TRIDENT, VASE, EMERALD, PYRAM, PEARL, RUG, SPICES, CHAIN};

enum movement {BACK = 8, NULLMOVE = 21, LOOK = 57, DEPRESSION = 63, ENTRANCE =
 64, CAVE = 67};

enum action {TAKE = 1, DROP, SAY, OPEN, NOTHING, LOCK, ON, OFF, WAVE, CALM,
 WALK, KILL, POUR, EAT, DRINK, RUB, THROW, QUIT, FIND, INVENT, FEED, FILL,
 BLAST, SCORE, FOO, BRIEF, READ, BREAK, WAKE, SUSPEND, HOURS, RESUME};

#define MAXDIE  3
#define CHLOC   114
#define CHLOC2  140

/* The longest word kept from a command (the original kept 80 characters;
   the display has 26). */
#define INLEN 24

/* The game's state: jwodder's struct advgame, with smaller types where
   the values allow. */
struct advgame {
    int loc, newloc, oldloc, oldloc2, limit;
    int turns, knifeloc, abbnum, clock1, clock2;
    unsigned char iwest, detail, numdie, holding;
    signed char foobar;
    unsigned char tally, tally2;
    bool wzdark, closing, lmwarn, panic, closed;
    signed char prop[65];
    unsigned char abb[141];
    int hintlc[10];
    bool hinted[10];
    unsigned char dloc[6];
    unsigned char odloc[6];
    bool dseen[6];
    unsigned char dflag, dkill;
    int place[65];
    int fixed[65];
    unsigned char atloc[141];
    unsigned char link[165];
};

extern struct advgame game;
extern int togoto;
extern bool blklin, gaveup;
extern int bonus;
extern int verb, obj;
extern char in1[INLEN + 1], in2[INLEN + 1];
extern char word1[6], word2[6];

/* main.c (fixed on the card; see BANKED_CE) */
void newGame (void) BANKED_CE;
void turn (void) BANKED_CE;
bool toting (int item) BANKED_CE;
bool here (int item) BANKED_CE;
bool at (int item) BANKED_CE;
int liq (void) BANKED_CE;
int liqloc (int loc) BANKED_CE;
bool bitset (int loc, int n) BANKED_CE;
bool forced (int loc) BANKED_CE;
bool dark (void) BANKED_CE;
bool pct (int x) BANKED_CE;
int ran (int max) BANKED_CE;
void seedRan (unsigned long seed) BANKED_CE;
void destroy (int obj) BANKED_CE;
void juggle (int obj) BANKED_CE;
void move (int obj, int where) BANKED_CE;
int put (int obj, int where, int pval) BANKED_CE;
void carry (int obj, int where) BANKED_CE;
void drop (int obj, int where) BANKED_CE;
bool dwarfHere (void) BANKED_CE;
void bug (int num) BANKED_CE;

/* io.c (fixed on the card; see BANKED_CE. io_host.c on a PC): the text */
void outc (char c) BANKED_CE;                 /* one character, word-wrapped */
void outs (const char *s) BANKED_CE;
void outn (int n) BANKED_CE;
void endpara (void) BANKED_CE;                /* end the line */
void say (unsigned int n) BANKED_CE;          /* message n as a paragraph of its own */
void sayc (unsigned int n) BANKED_CE;         /* message n, on from the current line */
void sayb (unsigned int n) BANKED_CE;         /* message n, beginning a line */
void rspeak (int msg) BANKED_CE;
void pspeak (int item, int state) BANKED_CE;
void getin (char *w1, char *r1, char *w2, char *r2) BANKED_CE;
bool yes (int x, int y, int z) BANKED_CE;
void finish (void) BANKED_CE;                 /* the last line read, back to BASIC */
/* the saved game (savegame.c, always fixed: it switches the bank, in
   bank SAVE_BANK, save.asm; a file on a PC, io_host.c) */
#define SAVE_BANK 0
#define SAVE_SIZE 1200
void saveGame (void);
bool loadGame (void);
bool haveSave (void);
void startSeed (void) BANKED_CE;              /* the title screen; seeds ran() */
extern bool savable;                /* BREAK saves the game */

/* vocab.c (bank 0) */
int vocab (const char *word, int type) BANKED;

/* verbs.c (bank 0) */
void intransitive (void) BANKED;
void transitive (void) BANKED;
void doaction (void) BANKED;
void vsay (void) BANKED;

/* turn.c (bank 3): the bigger parts of turn(), which stays in main.c.
   Each returns false where the original's turn() returned. */
bool describe (void) BANKED;
void hintsTurn (void) BANKED;
bool clocks (void) BANKED;

/* motion.c (bank 4, with the travel table) */
void dwarves (void) BANKED;         /* turn()'s first part: they move */
void domove (int motion) BANKED;
void death (void) BANKED;
int score (bool scoring) BANKED;
void normend (void) BANKED;

#endif
