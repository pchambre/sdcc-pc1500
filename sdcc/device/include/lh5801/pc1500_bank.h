/*-------------------------------------------------------------------------
   pc1500_bank.h - bank-switched 0000H-3FFFH for C programs on the PC-1500
   (LH5801 SDCC port).

   Paul Chambre, 2026

   Four devices put switchable 16K banks at 0000H-3FFFH:

     device                 banks      --banks=   code in a bank from
     internal card          BANK 0-4   card       00C5H
     CE-163 (Sharp)         0-1        ce163      00C5H
     CE-1638                0-7        ce1638     0104H (vendor switcher below)
     CE-163F                RAM 0-7,   ce163f     0112H (vendor switcher below)
                            flash 9-15

   A file's code goes in bank N with

       #pragma bank 2

   and its functions that other banks (or fixed code) call are declared
   __banked, in a header every caller includes:

       int score (int hand) __banked;

   A call to one goes through ___sdcc_bcall, which puts its bank in and
   the caller's back after; arguments and results are as for any call.
   A __banked function can't be reentrant or variadic, nor called through
   a pointer. main(), the library, every variable and every constant stay
   outside the banks (build-lh5801.sh puts them at the base address,
   4000H or above). Build with

       build-lh5801.sh --banks=ce1638 main.c bank1.c bank2.c 40C5

   which writes the fixed image and one image per bank, and LOAD.TXT,
   the steps to load them. A bank holding code can't hold a BASIC
   program too.

   At startup the bank that's in is noted, and exit (or main's return)
   puts it back.
-------------------------------------------------------------------------*/

#ifndef PC1500_BANK_H
#define PC1500_BANK_H

/* Switch the 0000H-3FFFH window to `bank`, raw: BASIC's program pointers
   stay as they are. A __banked call still puts its caller's bank back. */
void bank_select (unsigned char bank);

/* The bank that's in. */
unsigned char bank_current (void);

#endif
