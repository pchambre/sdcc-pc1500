/* Real printf() -> real PC-1500 LCD, end to end: stdio's printf()/vprintf()
   (device/lib/printf_large.c, vprintf.c) formats "Hello, %d!\n" through
   putchar() below, which calls straight into the ROM's own font/display
   routines (device/lib/lh5801/lcd_putchar.asm) -- no reimplemented font,
   per the standing rule that ROM1.BIN functionality is never duplicated.

   NOTE: build-lh5801.sh only links the .asm files under device/lib/
   lh5801 -- it does not yet pull in the C-source stdio support this
   demo needs (vprintf.c, printf_large.c, strlen.c, _mulint.c,
   _muluchar.c). Until it does, build by hand: compile+assemble this
   file and each of those five device/lib sources with sdcc -S -mlh5801
   and sdaslh5801, then sdld all the resulting .rel files together (the
   device/lib/lh5801 ones too) with CODE based right after
   HOME+GSINIT+GSFINAL+CONST and DATA based right after CODE (see
   build-lh5801.sh's own comment for why unbased areas can't just be
   left to sdld's encounter-order placement).
*/
#include <stdio.h>
#include <pc1500.h>

int putchar(int c) {
    if (c == '\n') {
        return c;
    }
    lcdPutChar((unsigned char) c);
    return c;
}

int main(void) {
    lcdClear();
    printf("Hello, %d!\n", 1500);

    while (!onKeyPressed()) {
        idleTick();
    }

    return 0;
}
