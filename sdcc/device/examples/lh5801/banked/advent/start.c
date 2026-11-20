/*-------------------------------------------------------------------------
   start.c - Colossal Cave Adventure for the PC-1500: main(), in fixed
   memory in every build (main.c may be in a bank: see advent.h).
-------------------------------------------------------------------------*/

#include "advent.h"

#ifdef HOST
int main (int argc, char **argv)
#else
int main (void)
#endif
{
#ifdef HOST
    extern void hostInit (int argc, char **argv);
    hostInit (argc, argv);
#endif
    startSeed ();
    newGame ();
    if (haveSave () && yes (M_RESUMEQ, 0, 0) && loadGame ()) {
        say (M_RESTORED);
        domove (NULLMOVE);
    } else {
        game.newloc = 1;
        game.limit = (game.hinted[3] = yes (65, 1, 0)) ? 1000 : 330;
    }
    for (;;)
        turn ();
}
