/* banked.h - the banked-code test's functions, in banks 1 and 2. */

#ifndef BANKED_H
#define BANKED_H

/* bank 1 (banked1.c) */
unsigned int b1_sum (unsigned int x, unsigned char a) __banked;
unsigned char b1_bank (void) __banked;
unsigned char b1_back (unsigned char a) __banked;
void b1_exit (void) __banked;

/* bank 2 (banked2.c) */
unsigned char b2_twice (unsigned char a) __banked;
unsigned long b2_long (unsigned long v) __banked;

/* fixed (banked.c) */
extern volatile unsigned char seen[4];
unsigned char fixed_inc (unsigned char a);

#endif
