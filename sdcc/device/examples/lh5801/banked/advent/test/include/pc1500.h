/* pc1500.h for lcdtest.sh: the part of the LH5801 port's pc1500.h that
   io.c uses, for a PC build of io.c (host_lcd.c implements it). */
#ifndef __PC1500_H
#define __PC1500_H 1

#include <stdlib.h>

void lcdPutChar (unsigned char c);
void lcdClear (void);
unsigned char keyWait (void);
unsigned char keyScan (void);
void idleTick (void);

#define PC1500_KEY_ENTER 0x0D
#define PC1500_KEY_CL    0x18
#define PC1500_KEY_UP    0x0B
#define PC1500_KEY_DOWN  0x0A
#define PC1500_KEY_BREAK 0x0E
#define PC1500_TICKS_PER_SECOND 360

#endif
