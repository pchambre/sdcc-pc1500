/*-------------------------------------------------------------------------
   turn.c - Colossal Cave Adventure for the PC-1500: the bigger parts of
   the original's turn() (jwodder's advent350.c, labels 2000, 2600 and
   2608), in a bank. main.c's turn() calls them in order; each returns
   false where the original returned from turn().
-------------------------------------------------------------------------*/

#ifndef HOST
#pragma bank 3
#endif
#include "advent.h"

/* Label 2000: the location's description and what's there. */
bool describe (void) BANKED
{
    unsigned int kk;
    int i;

    if (game.loc == 0) {
        death ();
        return false;
    }
    kk = T_SHORT + game.loc;
    if (game.abb[game.loc] % game.abbnum == 0 || !(cond[game.loc] & COND_SHORT))
        kk = T_LONG + game.loc;
    if (!forced (game.loc) && dark ()) {
        if (game.wzdark && pct (35)) {
            rspeak (23);
            game.oldloc2 = game.loc;
            death ();
            return false;
        }
        kk = T_RMSG + 16;
    }
    if (toting (BEAR))
        rspeak (141);
    say (kk);
    if (forced (game.loc)) {
        domove (1);
        return false;
    }
    if (game.loc == 33 && pct (25) && !game.closing)
        rspeak (8);
    if (!dark ()) {
        game.abb[game.loc]++;
        for (i = game.atloc[game.loc]; i != 0; i = game.link[i]) {
            int o = i > 100 ? i - 100 : i;

            if (o == STEPS && toting (NUGGET))
                continue;
            if (game.prop[o] < 0) {
                if (game.closed)
                    continue;
                game.prop[o] = o == RUG || o == CHAIN;
                game.tally--;
                if (game.tally == game.tally2 && game.tally != 0 && game.limit >= 35)
                    game.limit = 35;
            }
            pspeak (o, o == STEPS && game.loc == game.fixed[STEPS] ? 1 : game.prop[o]);
        }
    }
    return true;
}

/* Label 2600, up to reading the command: the hints, the closed cave's
   treasures. */
void hintsTurn (void) BANKED
{
    unsigned char hint;
    int i;

    for (hint = 4; hint < 10; hint++) {
        bool hintable = true;

        if (game.hinted[hint])
            continue;
        if (!bitset (game.loc, hint))
            game.hintlc[hint] = -1;
        game.hintlc[hint]++;
        if (game.hintlc[hint] < hints[hint][0])
            continue;
        switch (hint) {
        case 4:
            if (game.prop[GRATE] != 0 || here (KEYS)) {
                game.hintlc[hint] = 0;
                hintable = false;
            }
            break;
        case 5:
            if (!here (BIRD) || !toting (ROD) || obj != BIRD)
                hintable = false;
            break;
        case 6:
            if (!here (SNAKE) || here (BIRD)) {
                game.hintlc[hint] = 0;
                hintable = false;
            }
            break;
        case 7:
            if (game.atloc[game.loc] || game.atloc[game.oldloc]
                || game.atloc[game.oldloc2] || game.holding <= 1) {
                game.hintlc[hint] = 0;
                hintable = false;
            }
            break;
        case 8:
            if (game.prop[EMERALD] == -1 || game.prop[PYRAM] != -1) {
                game.hintlc[hint] = 0;
                hintable = false;
            }
            break;
        }
        if (hintable) {
            game.hintlc[hint] = 0;
            if (yes (hints[hint][2], 0, 54)) {
                sayb (M_HINT1);
                outn (hints[hint][1]);
                sayc (M_POINTS);
                endpara ();
                game.hinted[hint] = yes (175, hints[hint][3], 54);
                if (game.hinted[hint] && game.limit > 30)
                    game.limit += 30 * hints[hint][1];
            }
        }
    }
    if (game.closed) {
        if (game.prop[OYSTER] < 0 && toting (OYSTER))
            pspeak (OYSTER, 1);
        for (i = 1; i < 65; i++)
            if (toting (i) && game.prop[i] < 0)
                game.prop[i] = -1 - game.prop[i];
    }
    game.wzdark = dark ();
    if (0 < game.knifeloc && game.knifeloc != game.loc)
        game.knifeloc = 0;
    ran (1);    /* kick the generator */
}

/* Label 2608: the turn counted, the cave closing, the lamp. */
bool clocks (void) BANKED
{
    unsigned char i;

    game.foobar = game.foobar > 0 ? -game.foobar : 0;
    game.turns++;
    if (verb == SAY) {
        if (*word2)
            verb = 0;
        else {
            vsay ();
            return false;
        }
    }
    if (game.tally == 0 && 15 <= game.loc && game.loc != 33)
        game.clock1--;
    if (game.clock1 == 0) {
        game.prop[GRATE] = game.prop[FISSUR] = 0;
        for (i = 0; i < 6; i++) {
            game.dloc[i] = 0;
            game.dseen[i] = false;
        }
        move (TROLL, 0);
        move (TROLL + 100, 0);
        move (TROLL2, 117);
        move (TROLL2 + 100, 122);
        juggle (CHASM);
        if (game.prop[BEAR] != 3)
            destroy (BEAR);
        game.prop[CHAIN] = game.prop[AXE] = 0;
        game.fixed[CHAIN] = game.fixed[AXE] = 0;
        rspeak (129);
        game.clock1 = -1;
        game.closing = true;
        togoto = 19999;
        return false;
    }
    if (game.clock1 < 0)
        game.clock2--;
    if (game.clock2 == 0) {
        game.prop[BOTTLE] = put (BOTTLE, 115, 1);
        game.prop[PLANT] = put (PLANT, 115, 0);
        game.prop[OYSTER] = put (OYSTER, 115, 0);
        game.prop[LAMP] = put (LAMP, 115, 0);
        game.prop[ROD] = put (ROD, 115, 0);
        game.prop[DWARF] = put (DWARF, 115, 0);
        game.loc = game.oldloc = game.newloc = 115;
        put (GRATE, 116, 0);
        game.prop[SNAKE] = put (SNAKE, 116, 1);
        game.prop[BIRD] = put (BIRD, 116, 1);
        game.prop[CAGE] = put (CAGE, 116, 0);
        game.prop[ROD2] = put (ROD2, 116, 0);
        game.prop[PILLOW] = put (PILLOW, 116, 0);
        game.prop[MIRROR] = put (MIRROR, 115, 0);
        game.fixed[MIRROR] = 116;
        for (i = 1; i < 65; i++)
            if (toting (i))
                destroy (i);
        rspeak (132);
        game.closed = true;
        togoto = 2;
        return false;
    }
    if (game.prop[LAMP] == 1)
        game.limit--;
    if (game.limit <= 30 && here (BATTER) && game.prop[BATTER] == 0 && here (LAMP)) {
        rspeak (188);
        game.prop[BATTER] = 1;
        if (toting (BATTER))
            drop (BATTER, game.loc);
        game.limit += 2500;
        game.lmwarn = false;
    } else if (game.limit == 0) {
        game.limit = -1;
        game.prop[LAMP] = 0;
        if (here (LAMP))
            rspeak (184);
    } else if (game.limit < 0 && game.loc <= 8) {
        rspeak (185);
        gaveup = true;
        normend ();
    } else if (game.limit <= 30 && !game.lmwarn && here (LAMP)) {
        game.lmwarn = true;
        rspeak (game.place[BATTER] == 0 ? 183 : game.prop[BATTER] == 1 ? 189 : 187);
    }
    return true;
}
