/*-------------------------------------------------------------------------
   verbs.c - Colossal Cave Adventure for the PC-1500: the verbs (jwodder's
   verbs.c, and doaction() from motion.c), in a bank. SUSPEND and RESUME
   keep the game in the card's RAM instead of a file.
-------------------------------------------------------------------------*/

#ifndef HOST
#pragma bank 0
#endif
#include <string.h>
#include "advent.h"

static void vtake (void);
static void vopen (void);
static void vread (void);
static void vkill (void);
static void vpour (void);
static void vdrink (void);
static void vfill (void);
static void vblast (void);
static void von (void);
static void voff (void);
static void vdrop (void);
static void vfeed (void);
static void vsuspend (void);
static void vresume (void);

/* "<word> what?", and the command is read again */
static void what (void)
{
    endpara ();
    outs (in1);
    sayc (M_WHAT);
    endpara ();
    obj = 0;
    togoto = 2600;
}

void doaction (void) BANKED
{
    if (*word2) {
        strcpy (word1, word2);
        strcpy (in1, in2);
        *word2 = *in2 = 0;
        togoto = 2610;
    } else if (verb)
        transitive ();
    else {
        sayb (M_WHATDO);
        outs (in1);
        sayc (M_QMARK);
        endpara ();
        togoto = 2600;
    }
}

/* Label 4080: a verb on its own. */
void intransitive (void) BANKED
{
    togoto = 2012;
    switch (verb) {
    case NOTHING:
        rspeak (54);
        break;
    case WALK:
        rspeak (actspk[verb]);
        break;

    case DROP: case SAY: case WAVE: case CALM: case RUB:
    case THROW: case FIND: case FEED: case BREAK: case WAKE:
        what ();
        break;

    case TAKE:
        if (!(game.atloc[game.loc] && game.link[game.atloc[game.loc]] == 0)
            || (game.dflag >= 2 && dwarfHere ()))
            what ();
        else {
            obj = game.atloc[game.loc];
            vtake ();
        }
        break;

    case OPEN:
    case LOCK:
        if (here (CLAM))
            obj = CLAM;
        if (here (OYSTER))
            obj = OYSTER;
        if (at (DOOR))
            obj = DOOR;
        if (at (GRATE))
            obj = GRATE;
        if (obj != 0 && here (CHAIN))
            what ();
        else {
            if (here (CHAIN))
                obj = CHAIN;
            if (obj == 0)
                rspeak (28);
            else
                vopen ();
        }
        break;

    case EAT:
        if (here (FOOD)) {
            destroy (FOOD);
            rspeak (72);
        } else
            what ();
        break;

    case QUIT:
        if ((gaveup = yes (22, 54, 54)))
            normend ();
        break;

    case INVENT: {
        int spk = 98;
        unsigned char i;

        for (i = 0; i < 65; i++) {
            if (i == BEAR || !toting (i))
                continue;
            if (spk == 98)
                rspeak (99);
            blklin = false;
            pspeak (i, -1);
            blklin = true;
            spk = 0;
        }
        if (toting (BEAR))
            spk = 141;
        rspeak (spk);
        break;
    }

    case SCORE:
        sayb (M_WOULDSCORE);
        outn (score (true));
        sayc (M_OF350);
        endpara ();
        if ((gaveup = yes (143, 54, 54)))
            normend ();
        break;

    case FOO: {
        int k = vocab (word1, 3);

        if (game.foobar == 1 - k) {
            game.foobar = k;
            if (k != 4) {
                rspeak (54);
                return;
            }
            game.foobar = 0;
            if (game.place[EGGS] == 92 || (toting (EGGS) && game.loc == 92))
                rspeak (42);
            else {
                if (game.place[EGGS] == 0 && game.place[TROLL] == 0
                    && game.prop[TROLL] == 0)
                    game.prop[TROLL] = 1;
                k = (game.loc == 92 ? 0 : here (EGGS) ? 1 : 2);
                move (EGGS, 92);
                pspeak (EGGS, k);
            }
        } else
            rspeak (game.foobar ? 151 : 42);
        break;
    }

    case BRIEF:
        game.abbnum = 10000;
        game.detail = 3;
        rspeak (156);
        break;

    case READ:
        if (here (MAGZIN))
            obj = MAGZIN;
        if (here (TABLET))
            obj = obj * 100 + TABLET;
        if (here (MESSAG))
            obj = obj * 100 + MESSAG;
        if (game.closed && toting (OYSTER))
            obj = OYSTER;
        if (obj > 100 || obj == 0 || dark ())
            what ();
        else
            vread ();
        break;

    case SUSPEND:
        vsuspend ();
        break;
    case RESUME:
        vresume ();
        break;
    case HOURS:
        say (M_OPENALL);
        break;

    case ON: von (); break;
    case OFF: voff (); break;
    case KILL: vkill (); break;
    case POUR: vpour (); break;
    case DRINK: vdrink (); break;
    case FILL: vfill (); break;
    case BLAST: vblast (); break;
    default: bug (23);
    }
}

/* Label 4090: a verb and its object. */
void transitive (void) BANKED
{
    togoto = 2012;
    switch (verb) {
    case TAKE: vtake (); break;
    case DROP: vdrop (); break;
    case SAY: vsay (); break;
    case OPEN: case LOCK: vopen (); break;
    case NOTHING: rspeak (54); break;
    case ON: von (); break;
    case OFF: voff (); break;

    case WAVE:
        if (!toting (obj) && !(obj == ROD && toting (ROD2)))
            rspeak (29);
        else if (obj != ROD || !at (FISSUR) || !toting (obj) || game.closing)
            rspeak (actspk[verb]);
        else {
            game.prop[FISSUR] = 1 - game.prop[FISSUR];
            pspeak (FISSUR, 2 - game.prop[FISSUR]);
        }
        break;

    case CALM: case WALK: case QUIT: case SCORE:
    case FOO: case BRIEF: case HOURS:
        rspeak (actspk[verb]);
        break;

    case KILL: vkill (); break;
    case POUR: vpour (); break;

    case EAT:
        if (obj == FOOD) {
            destroy (FOOD);
            rspeak (72);
        } else if (obj == BIRD || obj == SNAKE || obj == CLAM || obj == OYSTER
                   || obj == DWARF || obj == DRAGON || obj == TROLL || obj == BEAR)
            rspeak (71);
        else
            rspeak (actspk[verb]);
        break;

    case DRINK: vdrink (); break;
    case RUB: rspeak (obj == LAMP ? actspk[verb] : 76); break;

    case THROW:
        if (toting (ROD2) && obj == ROD && !toting (ROD))
            obj = ROD2;
        if (!toting (obj))
            rspeak (actspk[verb]);
        else if (50 <= obj && obj < 65 && at (TROLL)) {
            drop (obj, 0);
            move (TROLL, 0);
            move (TROLL + 100, 0);
            drop (TROLL2, 117);
            drop (TROLL2 + 100, 122);
            juggle (CHASM);
            rspeak (159);
        } else if (obj == FOOD && here (BEAR)) {
            obj = BEAR;
            vfeed ();
        } else if (obj == AXE) {
            signed char i = -1;
            unsigned char j;

            for (j = 0; j < 5; j++)
                if (game.dloc[j] == game.loc) {
                    i = j;
                    break;
                }
            if (i != -1) {
                if (ran (3) == 0)
                    rspeak (48);
                else {
                    game.dseen[i] = false;
                    game.dloc[i] = 0;
                    rspeak (++game.dkill == 1 ? 149 : 47);
                }
                drop (AXE, game.loc);
                domove (NULLMOVE);
            } else if (at (DRAGON) && game.prop[DRAGON] == 0) {
                rspeak (152);
                drop (AXE, game.loc);
                domove (NULLMOVE);
            } else if (at (TROLL)) {
                rspeak (158);
                drop (AXE, game.loc);
                domove (NULLMOVE);
            } else if (here (BEAR) && game.prop[BEAR] == 0) {
                drop (AXE, game.loc);
                game.fixed[AXE] = -1;
                game.prop[AXE] = 1;
                juggle (BEAR);  /* Don't try this at home, kids. */
                rspeak (164);
            } else {
                obj = 0;
                vkill ();
            }
        } else
            vdrop ();
        break;

    case FIND:
    case INVENT:
        if (toting (obj))
            rspeak (24);
        else if (game.closed)
            rspeak (138);
        else if (obj == DWARF && game.dflag >= 2 && dwarfHere ())
            rspeak (94);
        else if (at (obj) || (liq () == obj && at (BOTTLE)) || obj == liqloc (game.loc))
            rspeak (94);
        else
            rspeak (actspk[verb]);
        break;

    case FEED: vfeed (); break;
    case FILL: vfill (); break;
    case BLAST: vblast (); break;
    case READ: vread (); break;

    case BREAK:
        if (obj == VASE && game.prop[VASE] == 0) {
            if (toting (VASE))
                drop (VASE, game.loc);
            game.prop[VASE] = 2;
            game.fixed[VASE] = -1;
            rspeak (198);
        } else if (obj != MIRROR)
            rspeak (actspk[verb]);
        else if (!game.closed)
            rspeak (148);
        else {
            rspeak (197);
            rspeak (136);
            normend ();
        }
        break;

    case WAKE:
        if (obj == DWARF && game.closed) {
            rspeak (199);
            rspeak (136);
            normend ();
        } else
            rspeak (actspk[verb]);
        break;

    case SUSPEND: vsuspend (); break;
    case RESUME: vresume (); break;
    default: bug (24);
    }
}

static void vtake (void)
{
    int spk = 25, k;

    if (toting (obj)) {
        rspeak (actspk[verb]);
        return;
    }
    if (obj == PLANT && game.prop[PLANT] <= 0)
        spk = 115;
    if (obj == BEAR && game.prop[BEAR] == 1)
        spk = 169;
    if (obj == CHAIN && game.prop[BEAR] != 0)
        spk = 170;
    if (game.fixed[obj]) {
        rspeak (spk);
        return;
    }
    if (obj == WATER || obj == OIL) {
        if (!here (BOTTLE) || liq () != obj) {
            obj = BOTTLE;
            if (toting (BOTTLE) && game.prop[BOTTLE] == 1)
                vfill ();
            else {
                if (game.prop[BOTTLE] != 1)
                    spk = 105;
                if (!toting (BOTTLE))
                    spk = 104;
                rspeak (spk);
            }
            return;
        }
        obj = BOTTLE;
    }
    if (game.holding >= 7) {
        rspeak (92);
        return;
    }
    if (obj == BIRD && game.prop[BIRD] == 0) {
        if (toting (ROD)) {
            rspeak (26);
            return;
        }
        if (!toting (CAGE)) {
            rspeak (27);
            return;
        }
        game.prop[BIRD] = 1;
    }
    if ((obj == BIRD || obj == CAGE) && game.prop[BIRD] != 0)
        carry (BIRD + CAGE - obj, game.loc);
    carry (obj, game.loc);
    k = liq ();
    if (obj == BOTTLE && k != 0)
        game.place[k] = -1;
    rspeak (54);
}

static void vopen (void)
{
    int spk = actspk[verb];

    switch (obj) {
    case CLAM:
    case OYSTER: {
        int k = (obj == OYSTER);

        spk = 124 + k;
        if (toting (obj))
            spk = 120 + k;
        if (!toting (TRIDENT))
            spk = 122 + k;
        if (verb == LOCK)
            spk = 61;
        if (spk == 124) {
            destroy (CLAM);
            drop (OYSTER, game.loc);
            drop (PEARL, 105);
        }
        break;
    }
    case DOOR:
        spk = game.prop[DOOR] == 1 ? 54 : 111;
        break;
    case CAGE:
        spk = 32;
        break;
    case KEYS:
        spk = 55;
        break;
    case CHAIN:
        if (!here (KEYS))
            spk = 31;
        else if (verb == LOCK) {
            spk = 172;
            if (game.prop[CHAIN] != 0)
                spk = 34;
            if (game.loc != 130)
                spk = 173;
            if (spk == 172) {
                game.prop[CHAIN] = 2;
                if (toting (CHAIN))
                    drop (CHAIN, game.loc);
                game.fixed[CHAIN] = -1;
            }
        } else {
            spk = 171;
            if (game.prop[BEAR] == 0)
                spk = 41;
            if (game.prop[CHAIN] == 0)
                spk = 37;
            if (spk == 171) {
                game.prop[CHAIN] = game.fixed[CHAIN] = 0;
                if (game.prop[BEAR] != 3)
                    game.prop[BEAR] = 2;
                game.fixed[BEAR] = 2 - game.prop[BEAR];
            }
        }
        break;
    case GRATE:
        if (!here (KEYS))
            spk = 31;
        else if (game.closing) {
            spk = 130;
            if (!game.panic)
                game.clock2 = 15;
            game.panic = true;
        } else {
            spk = 34 + game.prop[GRATE];
            game.prop[GRATE] = (verb != LOCK);
            spk += 2 * game.prop[GRATE];
        }
        break;
    }
    rspeak (spk);
}

static void vread (void)
{
    if (dark ()) {
        sayb (M_ISEENO);
        outs (in1);
        sayc (M_HERE);
        endpara ();
    } else {
        int spk = actspk[verb];

        if (obj == MAGZIN)
            spk = 190;
        if (obj == TABLET)
            spk = 196;
        if (obj == MESSAG)
            spk = 191;
        if (obj == OYSTER && game.hinted[2] && toting (OYSTER))
            spk = 194;
        if (obj != OYSTER || game.hinted[2] || !toting (OYSTER) || !game.closed)
            rspeak (spk);
        else
            game.hinted[2] = yes (192, 193, 54);
    }
}

static void vkill (void)
{
    unsigned char i;

    if (obj == 0) {
        if (game.dflag >= 2 && dwarfHere ())
            obj = DWARF;
        if (here (SNAKE))
            obj = obj * 100 + SNAKE;
        if (at (DRAGON) && game.prop[DRAGON] == 0)
            obj = obj * 100 + DRAGON;
        if (at (TROLL))
            obj = obj * 100 + TROLL;
        if (here (BEAR) && game.prop[BEAR] == 0)
            obj = obj * 100 + BEAR;
        if (obj > 100) {
            what ();
            return;
        }
        if (obj == 0) {
            if (here (BIRD) && verb != THROW)
                obj = BIRD;
            if (here (CLAM) || here (OYSTER))
                obj = obj * 100 + CLAM;
            if (obj > 100) {
                what ();
                return;
            }
        }
    }
    switch (obj) {
    case BIRD:
        if (game.closed)
            rspeak (137);
        else {
            destroy (BIRD);
            game.prop[BIRD] = 0;
            if (game.place[SNAKE] == 19)
                game.tally2++;
            rspeak (45);
        }
        break;
    case 0:
        rspeak (44);
        break;
    case CLAM: case OYSTER:
        rspeak (150);
        break;
    case SNAKE:
        rspeak (46);
        break;
    case DWARF:
        if (game.closed) {
            rspeak (136);
            normend ();
        } else
            rspeak (49);
        break;
    case DRAGON:
        if (game.prop[DRAGON] != 0)
            rspeak (167);
        else {
            rspeak (49);
            verb = obj = 0;
            getin (word1, in1, word2, in2);
            if (strcmp (word1, "YES") && strcmp (word1, "Y")) {
                togoto = 2608;
                return;
            }
            pspeak (DRAGON, 1);
            game.prop[DRAGON] = 2;
            game.prop[RUG] = 0;
            move (DRAGON + 100, -1);
            move (RUG + 100, 0);
            move (DRAGON, 120);
            move (RUG, 120);
            for (i = 0; i < 65; i++)
                if (game.place[i] == 119 || game.place[i] == 121)
                    move (i, 120);
            game.loc = 120;
            domove (NULLMOVE);
        }
        break;
    case TROLL:
        rspeak (157);
        break;
    case BEAR:
        rspeak (165 + (game.prop[BEAR] + 1) / 2);
        break;
    default:
        rspeak (actspk[verb]);
    }
}

static void vpour (void)
{
    if (obj == BOTTLE || obj == 0)
        obj = liq ();
    if (obj == 0)
        what ();
    else if (!toting (obj))
        rspeak (actspk[verb]);
    else if (obj != OIL && obj != WATER)
        rspeak (78);
    else {
        game.prop[BOTTLE] = 1;
        game.place[obj] = 0;
        if (at (DOOR)) {
            game.prop[DOOR] = (obj == OIL);
            rspeak (113 + game.prop[DOOR]);
        } else if (at (PLANT)) {
            if (obj != WATER)
                rspeak (112);
            else {
                pspeak (PLANT, game.prop[PLANT] + 1);
                game.prop[PLANT] = (game.prop[PLANT] + 2) % 6;
                game.prop[PLANT2] = game.prop[PLANT] / 2;
                domove (NULLMOVE);
            }
        } else
            rspeak (77);
    }
}

static void vdrink (void)
{
    if (obj == 0 && liqloc (game.loc) != WATER && (liq () != WATER || !here (BOTTLE)))
        what ();
    else if (obj == 0 || obj == WATER) {
        if (liq () == WATER && here (BOTTLE)) {
            game.prop[BOTTLE] = 1;
            game.place[WATER] = 0;
            rspeak (74);
        } else
            rspeak (actspk[verb]);
    } else
        rspeak (110);
}

static void vfill (void)
{
    if (obj == VASE) {
        if (liqloc (game.loc) == 0)
            rspeak (144);
        else if (!toting (VASE))
            rspeak (29);
        else {
            rspeak (145);
            game.prop[VASE] = 2;
            game.fixed[VASE] = -1;
            /* jwodder: the original's bug, kept -- the vase stays whole
               if the pillow is here, but is fixed in place either way. */
            if (at (PILLOW))
                game.prop[VASE] = 0;
            pspeak (VASE, game.prop[VASE] + 1);
            drop (obj, game.loc);
        }
    } else {
        if (obj != 0 && obj != BOTTLE)
            rspeak (actspk[verb]);
        else if (obj == 0 && !here (BOTTLE))
            what ();
        else if (liq () != 0)
            rspeak (105);
        else if (liqloc (game.loc) == 0)
            rspeak (106);
        else {
            game.prop[BOTTLE] = cond[game.loc] & 2;
            if (toting (BOTTLE))
                game.place[liq ()] = -1;
            rspeak (liq () == OIL ? 108 : 107);
        }
    }
}

static void vblast (void)
{
    if (game.prop[ROD2] < 0 || !game.closed)
        rspeak (actspk[verb]);
    else {
        bonus = 133;
        if (game.loc == 115)
            bonus = 134;
        if (here (ROD2))
            bonus = 135;
        rspeak (bonus);
        normend ();
    }
}

static void von (void)
{
    if (!here (LAMP))
        rspeak (actspk[verb]);
    else if (game.limit < 0)
        rspeak (184);
    else {
        game.prop[LAMP] = 1;
        rspeak (39);
        if (game.wzdark)
            togoto = 2000;
    }
}

static void voff (void)
{
    if (!here (LAMP))
        rspeak (actspk[verb]);
    else {
        game.prop[LAMP] = 0;
        rspeak (40);
        if (dark ())
            rspeak (16);
    }
}

static void vdrop (void)
{
    int k;

    if (toting (ROD2) && obj == ROD && !toting (ROD))
        obj = ROD2;
    if (!toting (obj)) {
        rspeak (actspk[verb]);
        return;
    }
    if (obj == BIRD && here (SNAKE)) {
        rspeak (30);
        if (game.closed) {
            rspeak (136);
            normend ();
        }
        destroy (SNAKE);
        game.prop[SNAKE] = 1;
    } else if (obj == COINS && here (VEND)) {
        destroy (COINS);
        drop (BATTER, game.loc);
        pspeak (BATTER, 0);
        return;
    } else if (obj == BIRD && at (DRAGON) && game.prop[DRAGON] == 0) {
        rspeak (154);
        destroy (BIRD);
        game.prop[BIRD] = 0;
        if (game.place[SNAKE] == 19)
            game.tally2++;
        return;
    } else if (obj == BEAR && at (TROLL)) {
        rspeak (163);
        move (TROLL, 0);
        move (TROLL + 100, 0);
        move (TROLL2, 117);
        move (TROLL2 + 100, 122);
        juggle (CHASM);
        game.prop[TROLL] = 2;
    } else if (obj == VASE && game.loc != 96) {
        game.prop[VASE] = at (PILLOW) ? 0 : 2;
        pspeak (VASE, game.prop[VASE] + 1);
        if (game.prop[VASE] != 0)
            game.fixed[VASE] = -1;
    } else
        rspeak (54);
    k = liq ();
    if (k == obj)
        obj = BOTTLE;
    if (obj == BOTTLE && k != 0)
        game.place[k] = 0;
    if (obj == CAGE && game.prop[BIRD] != 0)
        drop (BIRD, game.loc);
    if (obj == BIRD)
        game.prop[BIRD] = 0;
    drop (obj, game.loc);
}

static void vfeed (void)
{
    switch (obj) {
    case BIRD:
        rspeak (100);
        break;
    case SNAKE:
        if (!game.closed && here (BIRD)) {
            destroy (BIRD);
            game.prop[BIRD] = 0;
            game.tally2++;
            rspeak (101);
        } else
            rspeak (102);
        break;
    case TROLL:
        rspeak (182);
        break;
    case DRAGON:
        rspeak (game.prop[DRAGON] != 0 ? 110 : 102);
        break;
    case DWARF:
        if (!here (FOOD))
            rspeak (actspk[verb]);
        else {
            game.dflag++;
            rspeak (103);
        }
        break;
    case BEAR:
        if (!here (FOOD))
            rspeak (game.prop[BEAR] == 0 ? 102 : game.prop[BEAR] == 3 ? 110 : actspk[verb]);
        else {
            destroy (FOOD);
            game.prop[BEAR] = 1;
            game.fixed[AXE] = 0;
            game.prop[AXE] = 0;
            rspeak (168);
        }
        break;
    default:
        rspeak (14);
    }
}

void vsay (void) BANKED
{
    char *tk = *in2 ? in2 : in1;
    int i;

    if (*word2)
        strcpy (word1, word2);
    i = vocab (word1, -1);
    if (i == 62 || i == 65 || i == 71 || i == 2025) {
        *word2 = 0;
        obj = 0;
        togoto = 2630;
    } else {
        sayb (M_OKAY);
        outs (tk);
        sayc (M_QUOTE);
        endpara ();
    }
}

/* SUSPEND: the game to the card's RAM, and back to BASIC. */
static void vsuspend (void)
{
    say (M_SUSPEND);
    if (!yes (200, 54, 54))
        return;
    saveGame ();
    say (M_SAVED);
    finish ();
}

static void vresume (void)
{
    if (!haveSave ()) {
        say (M_NOSAVE);
        return;
    }
    if (game.turns > 1) {
        say (M_ABANDON);
        if (!yes (200, 54, 54))
            return;
    }
    loadGame ();
    say (M_RESTORED);
    domove (NULLMOVE);
}
