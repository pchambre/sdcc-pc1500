/*-------------------------------------------------------------------------
   savegame.c - Colossal Cave Adventure for the PC-1500: the saved game,
   copied between the game's variables and bank SAVE_BANK (save.asm). In
   fixed memory in every build: it switches the bank.
-------------------------------------------------------------------------*/

#include <string.h>
#include <pc1500_bank.h>
#include "advent.h"

#define SAVE_MAGIC 0xAD35

extern unsigned char save_area[];      /* save.asm: SAVE_SIZE bytes */

/* the sum and the game fit */
typedef char save_fits[sizeof (struct advgame) + 2 <= SAVE_SIZE ? 1 : -1];

static unsigned int sum (const unsigned char *p, unsigned int n)
{
    unsigned int s = SAVE_MAGIC;

    while (n--)
        s = (s << 1 | s >> 15) + *p++;
    return s;
}

enum {SAVE_CHECK, SAVE_LOAD, SAVE_STORE};

/* Between fixed memory and the save bank, putting back the bank that
   was in -- the caller's, if a __banked function called this. Checking
   or loading: whether a saved game is there (its sum matches). */
static bool saveCopy (unsigned char how)
{
    unsigned char was = bank_current ();
    unsigned int s;
    bool ok = true;

    bank_select (SAVE_BANK);
    if (how == SAVE_STORE) {
        s = sum ((const unsigned char *) &game, sizeof game);
        memcpy (save_area + 2, &game, sizeof game);
        save_area[0] = s >> 8;
        save_area[1] = s;
    } else {
        s = (save_area[0] << 8) | save_area[1];
        ok = s == sum (save_area + 2, sizeof game);
        if (ok && how == SAVE_LOAD)
            memcpy (&game, save_area + 2, sizeof game);
    }
    bank_select (was);
    return ok;
}

void saveGame (void)
{
    saveCopy (SAVE_STORE);
}

bool loadGame (void)
{
    return saveCopy (SAVE_LOAD);
}

bool haveSave (void)
{
    return saveCopy (SAVE_CHECK);
}
