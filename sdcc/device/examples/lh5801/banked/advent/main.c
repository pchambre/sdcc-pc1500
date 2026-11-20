/*-------------------------------------------------------------------------
   main.c - Colossal Cave Adventure for the PC-1500: turn() and the small
   helpers every bank calls (fixed memory on the card, bank 4 on a
   CE-163F: see advent.h's BANKED_CE). From jwodder's advent350.c and
   util.c; see advent.h. main() is in start.c.
-------------------------------------------------------------------------*/

#if defined ADV_CE163F && !defined HOST
#pragma bank 4          /* see advent.h's BANKED_CE */
#endif

#include <string.h>
#include "advent.h"

int togoto = 2;
bool blklin = true, gaveup;
int bonus;
int verb, obj;
char in1[INLEN + 1], in2[INLEN + 1];
char word1[6], word2[6];
struct advgame game;

/* the objects' starting places (the original's initializer for place[]
   and fixed[]) */
static const unsigned char place0[65] = {
    0, 3, 3, 8, 10, 11, 0, 14, 13, 94,
    96, 19, 17, 101, 103, 0, 106, 0, 0, 3,
    3, 0, 0, 109, 25, 23, 111, 35, 0, 97,
    0, 119, 117, 117, 0, 130, 0, 126, 140, 0,
    96, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    18, 27, 28, 29, 30, 0, 92, 95, 97, 100,
    101, 0, 119, 127, 130
};
static const signed char fixed0[65] = {
    0, 0, 0, 9, 0, 0, 0, 15, 0, -1,
    0, -1, 27, -1, 0, 0, 0, -1, 0, 0,
    0, 0, 0, -1, -1, 67, -1, 110, 0, -1,
    -1, 121, 122, 122, 0, -1, -1, -1, -1, 0,
    -1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 121, 0, -1
};
static const unsigned char dloc0[6] = {19, 27, 33, 44, 64, CHLOC};

void newGame (void) BANKED_CE
{
    unsigned char k;

    memset (&game, 0, sizeof game);
    game.tally = 15;
    game.abbnum = 5;
    game.clock1 = 30;
    game.clock2 = 50;
    for (k = 50; k < 65; k++)
        game.prop[k] = -1;
    memcpy (game.dloc, dloc0, sizeof dloc0);
    for (k = 0; k < 65; k++) {
        game.place[k] = place0[k];
        game.fixed[k] = fixed0[k];
    }
    for (k = 64; k > 0; k--) {
        if (game.fixed[k] > 0) {
            drop (k + 100, game.fixed[k]);
            drop (k, game.place[k]);
        }
    }
    for (k = 64; k > 0; k--) {
        if (game.place[k] && game.fixed[k] <= 0)
            drop (k, game.place[k]);
    }
}

static void seeno (void)
{
    sayb (M_ISEENO);
    outs (in1);
    sayc (M_HERE);
    endpara ();
    togoto = 2012;
}

/* The original's turn(), its bigger parts in banks (turn.c, motion.c).
   Falling into the next case is the same as setting togoto to it and
   returning, which is how those parts end. */
void turn (void) BANKED_CE
{
    switch (togoto) {
    case 2:
        dwarves ();
        return;

    case 2000:
        if (!describe ())
            return;

    case 2012:
        verb = obj = 0;

    case 2600:
        hintsTurn ();
        savable = true;
        getin (word1, in1, word2, in2);

    case 2608:
        if (!clocks ())
            return;

    case 19999:
        if (strcmp (word1, "ENTER") == 0 && (strcmp (word2, "STREA") == 0
            || strcmp (word2, "WATER") == 0)) {
            rspeak (liqloc (game.loc) == WATER ? 70 : 43);
            togoto = 2012;
            return;
        }
        if (strcmp (word1, "ENTER") == 0 && *word2) {
            strcpy (word1, word2);
            strcpy (in1, in2);
            *word2 = *in2 = 0;
        } else if ((strcmp (word1, "WATER") == 0 || strcmp (word1, "OIL") == 0)
                   && (strcmp (word2, "PLANT") == 0 || strcmp (word2, "DOOR") == 0)) {
            if (at (vocab (word2, 1)))
                strcpy (word2, "POUR");
        }

    case 2610:
        if (strcmp (word1, "WEST") == 0 && ++game.iwest == 10)
            rspeak (17);

    case 2630: {
        int i = vocab (word1, -1);
        int k;

        if (i == -1) {
            rspeak (pct (20) ? 61 : pct (20) ? 13 : 60);
            togoto = 2600;
            return;
        }
        k = i % 1000;
        switch (i / 1000) {
        case 0:
            domove (k);
            break;
        case 1:
            obj = k;
            if (game.fixed[obj] == game.loc || here (obj))
                doaction ();
            else if (obj == GRATE) {
                if (game.loc == 1 || game.loc == 4 || game.loc == 7)
                    k = DEPRESSION;
                if (9 < game.loc && game.loc < 15)
                    k = ENTRANCE;
                if (k != GRATE)
                    domove (k);
                else if ((verb == FIND || verb == INVENT) && !*word2)
                    doaction ();
                else
                    seeno ();
            } else if ((obj == DWARF && game.dflag >= 2 && dwarfHere ())
                       || (obj == liq () && here (BOTTLE))
                       || obj == liqloc (game.loc)) {
                doaction ();
            } else if (obj == PLANT && at (PLANT2) && game.prop[PLANT2] != 0) {
                obj = PLANT2;
                doaction ();
            } else if (obj == KNIFE && game.knifeloc == game.loc) {
                game.knifeloc = -1;
                rspeak (116);
                togoto = 2012;
            } else if (obj == ROD && here (ROD2)) {
                obj = ROD2;
                doaction ();
            } else if ((verb == FIND || verb == INVENT) && !*word2)
                doaction ();
            else
                seeno ();
            break;
        case 2:
            verb = k;
            if (verb == SAY || verb == SUSPEND || verb == RESUME)
                obj = *word2;   /* only whether an object was given */
            else if (*word2) {
                strcpy (word1, word2);
                strcpy (in1, in2);
                *word2 = *in2 = 0;
                togoto = 2610;
                return;
            }
            if (obj)
                transitive ();
            else
                intransitive ();
            break;
        case 3:
            rspeak (k);
            togoto = 2012;
            break;
        default:
            bug (22);
        }
    }
    }
}

/*----- util.c's helpers ------------------------------------------------*/

bool toting (int item) BANKED_CE
{
    return game.place[item] == -1;
}

bool here (int item) BANKED_CE
{
    return game.place[item] == game.loc || toting (item);
}

bool at (int item) BANKED_CE
{
    return game.loc == game.place[item] || game.loc == game.fixed[item];
}

static int liq2 (int p)
{
    return p == 0 ? WATER : p == 2 ? OIL : 0;
}

int liq (void) BANKED_CE
{
    return liq2 (game.prop[BOTTLE] < 0 ? -1 - game.prop[BOTTLE] : game.prop[BOTTLE]);
}

int liqloc (int loc) BANKED_CE
{
    return liq2 (cond[loc] & 4 ? cond[loc] & 2 : 1);
}

bool bitset (int loc, int n) BANKED_CE
{
    return (cond[loc] & (1 << n)) != 0;
}

bool forced (int loc) BANKED_CE
{
    return loc > 0 && (cond[loc] & COND_FORCED);
}

bool dark (void) BANKED_CE
{
    return !(cond[game.loc] & 1 || (game.prop[LAMP] && here (LAMP)));
}

bool pct (int x) BANKED_CE
{
    return ran (100) < x;
}

void destroy (int obj) BANKED_CE
{
    move (obj, 0);
}

void juggle (int obj) BANKED_CE
{
    move (obj, game.place[obj]);
    move (obj + 100, game.fixed[obj]);
}

void move (int obj, int where) BANKED_CE
{
    int from = obj > 100 ? game.fixed[obj - 100] : game.place[obj];

    if (0 < from && from <= 300)
        carry (obj, from);
    drop (obj, where);
}

int put (int obj, int where, int pval) BANKED_CE
{
    move (obj, where);
    return -1 - pval;
}

void carry (int obj, int where) BANKED_CE
{
    unsigned char tmp;

    if (obj <= 100) {
        if (game.place[obj] == -1)
            return;
        game.place[obj] = -1;
        game.holding++;
    }
    if (game.atloc[where] == obj)
        game.atloc[where] = game.link[obj];
    else {
        for (tmp = game.atloc[where]; game.link[tmp] != obj; tmp = game.link[tmp])
            ;
        game.link[tmp] = game.link[obj];
    }
}

void drop (int obj, int where) BANKED_CE
{
    if (obj > 100)
        game.fixed[obj - 100] = where;
    else {
        if (game.place[obj] == -1)
            game.holding--;
        game.place[obj] = where;
    }
    if (where > 0) {
        game.link[obj] = game.atloc[where];
        game.atloc[where] = obj;
    }
}

bool dwarfHere (void) BANKED_CE
{
    unsigned char i;

    for (i = 0; i < 5; i++)
        if (game.dloc[i] == game.loc)
            return true;
    return false;
}

/* 5 - Required vocabulary word not found
   20 - Special travel number (500>L>300) is outside of defined range
   22 - Vocabulary type (N/1000) not between 0 and 3
   23 - Intransitive action verb not defined
   24 - Transitive action verb not defined
   26 - Location has no travel entries */
void bug (int num) BANKED_CE
{
    sayb (M_BUG);
    outn (num);
    endpara ();
    finish ();
}

/* The original Fortran's generator (jwodder's ORIG_RNG). Not static:
   a test sets it (lcdtest.sh, through the map). */
unsigned long ranState;

void seedRan (unsigned long seed) BANKED_CE
{
    ranState = seed;
}

int ran (int max) BANKED_CE
{
    ranState = (ranState * 1021) & 0xFFFFFUL;
    return (int) (((unsigned long) max * ranState) >> 20);
}
