/*-------------------------------------------------------------------------
   motion.c - Colossal Cave Adventure for the PC-1500: moving, the
   dwarves and pirate, death and the score (jwodder's motion.c and the
   first part of advent350.c's turn()). In the bank with the travel
   table, which only this file reads.

   gen.py packs the original's travel[141][13][11]: an offset per
   location, then its rows -- the condition (the original's L / 1000, 2
   bytes), the destination (L % 1000, 2 bytes) and the motion words, the
   last one with bit 7 set -- ended by FFH.
-------------------------------------------------------------------------*/

#ifndef HOST
#pragma bank 4
#endif
#include "advent.h"

extern const unsigned char travel_data[];

static const unsigned char *rows (int loc)
{
    const unsigned char *p = travel_data + 2 * loc;

    return travel_data + 2 * 141 + ((p[0] << 8) | p[1]);
}

#define END(r)   ((r)[0] == 0xFF)
#define COND(r)  (((r)[0] << 8) | (r)[1])
#define DEST(r)  (((r)[2] << 8) | (r)[3])
#define FIRST(r) ((r)[4] & 0x7F)

static const unsigned char *next (const unsigned char *r)
{
    r += 4;
    while (!(*r++ & 0x80))
        ;
    return r;
}

/* The first part of turn() (label 2): the closing cave, the move made,
   the dwarves and the pirate. Goes on at label 2000. */
void dwarves (void) BANKED
{
    unsigned char i, j, dtotal, attack, stick;
    int k;

    if (0 < game.newloc && game.newloc < 9 && game.closing) {
        rspeak (130);
        game.newloc = game.loc;
        if (!game.panic)
            game.clock2 = 15;
        game.panic = true;
    }
    if (game.newloc != game.loc && !forced (game.loc) && !bitset (game.loc, 3)) {
        for (i = 0; i < 5; i++) {
            if (game.odloc[i] == game.newloc && game.dseen[i]) {
                game.newloc = game.loc;
                rspeak (2);
                break;
            }
        }
    }
    game.loc = game.newloc;
    togoto = 2000;
    if (game.loc == 0 || forced (game.loc) || bitset (game.newloc, 3))
        return;
    if (game.dflag == 0) {
        if (game.loc >= 15)
            game.dflag = 1;
        return;
    }
    if (game.dflag == 1) {
        if (game.loc < 15 || pct (95))
            return;
        game.dflag = 2;
        if (pct (50))
            game.dloc[ran (5)] = 0;
        if (pct (50))   /* Yes, this is supposed to be done twice. */
            game.dloc[ran (5)] = 0;
        for (i = 0; i < 5; i++) {
            if (game.dloc[i] == game.loc)
                game.dloc[i] = 18;
            game.odloc[i] = game.dloc[i];
        }
        rspeak (3);
        drop (AXE, game.loc);
        return;
    }
    dtotal = attack = stick = 0;
    for (i = 0; i < 6; i++) {   /* each dwarf (5: the pirate) moves */
        const unsigned char *r;
        int kk = 0, tk;

        if (game.dloc[i] == 0)
            continue;
        tk = game.odloc[i];
        for (r = rows (game.dloc[i]); !END (r); r = next (r)) {
            int newloc = DEST (r);

            if (15 <= newloc && newloc <= 300 && newloc != game.odloc[i]
                && newloc != game.dloc[i] && !forced (newloc)
                && !(i == 5 && bitset (newloc, 3)) && COND (r) != 100
                && ran (++kk) == 0)
                tk = newloc;
        }
        game.odloc[i] = game.dloc[i];
        game.dloc[i] = tk;
        game.dseen[i] = (game.dseen[i] && game.loc >= 15)
            || game.dloc[i] == game.loc || game.odloc[i] == game.loc;
        if (!game.dseen[i])
            continue;
        game.dloc[i] = game.loc;
        if (i == 5) {
            /* the pirate */
            bool here_ = false, stole = false;

            if (game.loc == CHLOC || game.prop[CHEST] >= 0)
                continue;
            for (j = 50; j < 65; j++) {
                if (j == PYRAM && (game.loc == 100 || game.loc == 101))
                    continue;
                if (toting (j)) {
                    rspeak (128);
                    if (game.place[MESSAG] == 0)
                        move (CHEST, CHLOC);
                    move (MESSAG, CHLOC2);
                    for (j = 50; j < 65; j++) {
                        if (j == PYRAM && (game.loc == 100 || game.loc == 101))
                            continue;
                        if (at (j) && game.fixed[j] == 0)
                            carry (j, game.loc);
                        if (toting (j))
                            drop (j, CHLOC);
                    }
                    game.dloc[5] = game.odloc[5] = CHLOC;
                    game.dseen[5] = false;
                    stole = true;
                    break;
                }
                if (here (j))
                    here_ = true;
            }
            if (!stole) {
                if (game.tally == game.tally2 + 1 && !here_ && game.place[CHEST] == 0
                    && here (LAMP) && game.prop[LAMP] == 1) {
                    rspeak (186);
                    move (CHEST, CHLOC);
                    move (MESSAG, CHLOC2);
                    game.dloc[5] = game.odloc[5] = CHLOC;
                    game.dseen[5] = false;
                } else if (game.odloc[5] != game.dloc[5] && pct (20))
                    rspeak (127);
            }
        } else {
            dtotal++;
            if (game.odloc[i] == game.dloc[i]) {
                attack++;
                if (game.knifeloc >= 0)
                    game.knifeloc = game.loc;
                if (ran (1000) < 95 * (game.dflag - 2))
                    stick++;
            }
        }
    }
    if (dtotal == 0)
        return;
    if (dtotal == 1)
        rspeak (4);
    else {
        sayb (M_DWARVES1);
        outn (dtotal);
        sayc (M_DWARVES2);
        endpara ();
    }
    if (attack == 0)
        return;
    if (game.dflag == 2)
        game.dflag = 3;
    if (attack == 1) {
        rspeak (5);
        k = 52;
    } else {
        endpara ();
        outn (attack);
        sayc (M_KNIVES);
        endpara ();
        k = 6;
    }
    if (stick <= 1) {
        rspeak (k + stick);
        if (stick == 0)
            return;
    } else {
        endpara ();
        outn (stick);
        sayc (M_GETYOU);
        endpara ();
    }
    game.oldloc2 = game.loc;
    death ();
}

static void dotrav (int motion)
{
    const unsigned char *r;
    int rdest = -1;
    bool matched = false;

    for (r = rows (game.loc); !END (r); r = next (r)) {
        if (!matched) {
            const unsigned char *m = r + 4;

            for (;;) {
                unsigned char w = *m & 0x7F;

                if (w == 1 || w == motion) {
                    matched = true;
                    break;
                }
                if (*m++ & 0x80)
                    break;
            }
        }
        if (matched) {
            int rcond = COND (r);
            int robject = rcond % 100;

            if (rcond == 0 || rcond == 100)
                rdest = DEST (r);
            else if (rcond < 100) {
                if (pct (rcond))
                    rdest = DEST (r);
            } else if (rcond <= 200) {
                if (toting (robject))
                    rdest = DEST (r);
            } else if (rcond <= 300) {
                if (toting (robject) || at (robject))
                    rdest = DEST (r);
            } else if (game.prop[robject] != rcond / 100 - 3)
                rdest = DEST (r);
            if (rdest != -1)
                break;
        }
    }
    if (rdest == -1) {
        switch (motion) {
        case 29: case 30: case 43: case 44: case 45:
        case 46: case 47: case 48: case 49: case 50:
            rspeak (9);
            break;
        case 7: case 36: case 37:
            rspeak (10);
            break;
        case 11: case 19:
            rspeak (11);
            break;
        case 62: case 65:
            rspeak (42);
            break;
        case 17:
            rspeak (80);
            break;
        default:
            rspeak (verb == FIND || verb == INVENT ? 59 : 12);
        }
    } else if (0 <= rdest && rdest <= 300)
        game.newloc = rdest;
    else if (rdest == 301) {
        if (!game.holding || (game.holding == 1 && toting (EMERALD)))
            game.newloc = 99 + 100 - game.loc;
        else {
            game.newloc = game.loc;
            rspeak (117);
        }
    } else if (rdest == 302) {
        drop (EMERALD, game.loc);
        game.newloc = (game.loc == 33 ? 100 : 33);
    } else if (rdest == 303) {
        if (game.prop[TROLL] == 1) {
            pspeak (TROLL, 1);
            game.prop[TROLL] = 0;
            move (TROLL2, 0);
            move (TROLL2 + 100, 0);
            move (TROLL, 117);
            move (TROLL + 100, 122);
            juggle (CHASM);
            game.newloc = game.loc;
        } else {
            game.newloc = (game.loc == 117 ? 122 : 117);
            if (game.prop[TROLL] == 0)
                game.prop[TROLL] = 1;
            if (toting (BEAR)) {
                rspeak (162);
                game.prop[CHASM] = 1;
                game.prop[TROLL] = 2;
                drop (BEAR, game.newloc);
                game.fixed[BEAR] = -1;
                game.prop[BEAR] = 3;
                if (game.prop[SPICES] < 0)
                    game.tally2++;
                game.oldloc2 = game.newloc;
                death ();
            }
        }
    } else if (rdest > 500)
        rspeak (rdest - 500);
    else
        bug (20);
}

void domove (int motion) BANKED
{
    togoto = 2;
    game.newloc = game.loc;
    if (END (rows (game.loc)))
        bug (26);
    switch (motion) {
    case NULLMOVE:
        return;
    case BACK: {
        int k = forced (game.oldloc) ? game.oldloc2 : game.oldloc;
        const unsigned char *r, *k2 = 0;

        game.oldloc2 = game.oldloc;
        game.oldloc = game.loc;
        if (k == game.loc) {
            rspeak (91);
            break;
        }
        for (r = rows (game.loc); !END (r); r = next (r)) {
            int ll = DEST (r);

            if (ll == k) {
                dotrav (FIRST (r));
                return;
            } else if (ll <= 300 && forced (ll) && DEST (rows (ll)) == k)
                k2 = r;
        }
        /* The original kept the row's index and tested it for 0, so a
           match in the first row was never taken. */
        if (k2 != 0 && k2 != rows (game.loc))
            dotrav (FIRST (k2));
        else
            rspeak (140);
        break;
    }
    case LOOK:
        if (game.detail++ < 3)
            rspeak (15);
        game.wzdark = false;
        game.abb[game.loc] = 0;
        break;
    case CAVE:
        rspeak (game.loc < 8 ? 57 : 58);
        break;
    default:
        game.oldloc2 = game.oldloc;
        game.oldloc = game.loc;
        dotrav (motion);
    }
}

void death (void) BANKED
{
    if (game.closing) {
        rspeak (131);
        game.numdie++;
        normend ();
    } else {
        bool yea = yes (81 + game.numdie * 2, 82 + game.numdie * 2, 54);
        unsigned char i;

        game.numdie++;
        if (game.numdie == MAXDIE || !yea)
            normend ();
        game.place[WATER] = game.place[OIL] = 0;
        if (toting (LAMP))
            game.prop[LAMP] = 0;
        for (i = 64; i > 0; i--) {
            if (toting (i))
                drop (i, i == LAMP ? 1 : game.oldloc2);
        }
        game.loc = game.oldloc = 3;
        togoto = 2000;
    }
}

int score (bool scoring) BANKED
{
    int scr = 0;
    unsigned char i;

    for (i = 50; i < 65; i++) {
        if (game.prop[i] >= 0)
            scr += 2;
        if (game.place[i] == 3 && game.prop[i] == 0)
            scr += (i == CHEST ? 12 : i > CHEST ? 14 : 10);
    }
    scr += (MAXDIE - game.numdie) * 10;
    if (!scoring && !gaveup)
        scr += 4;
    if (game.dflag != 0)
        scr += 25;
    if (game.closing)
        scr += 25;
    if (game.closed) {
        switch (bonus) {
        case 0: scr += 10; break;
        case 133: scr += 45; break;
        case 134: scr += 30; break;
        case 135: scr += 25; break;
        }
    }
    if (game.place[MAGZIN] == 108)
        scr++;
    scr += 2;
    for (i = 1; i < 10; i++)
        if (game.hinted[i])
            scr -= hints[i][1];
    return scr;
}

void normend (void) BANKED
{
    int scr = score (false);
    unsigned char i;

    sayb (M_SCORED1);
    outn (scr);
    sayc (M_SCORED2);
    outn (game.turns);
    sayc (M_TURN);
    if (game.turns != 1)
        sayc (M_S);
    sayc (M_DOT);
    endpara ();
    for (i = 0; i < NCLASSES; i++)
        if (classes[i] >= scr)
            break;
    if (i < NCLASSES) {
        say (T_CLASS + i);
        if (i + 1 < NCLASSES) {
            int diff = classes[i] - scr + 1;

            sayb (M_NEXT1);
            outn (diff);
            sayc (M_MOREPT);
            if (diff != 1)
                sayc (M_S);
            sayc (M_DOT);
            endpara ();
        } else {
            say (M_NEATTRICK);
            say (M_CONGRATS);
        }
    } else
        say (M_OFFSCALE);
    finish ();
}
