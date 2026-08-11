/* Real printf() -> real PC-1500 LCD, end to end: stdio's printf()/vprintf()
   (device/lib/printf_large.c, vprintf.c) formats "Hello, %d!\n" through
   putchar() below, which calls straight into the ROM's own font/display
   routines (device/lib/lh5801/lcd_putchar.asm) -- no reimplemented font,
   per the standing rule that ROM1.BIN functionality is never duplicated.

   Confirmed live via pc1500emu: builds and links cleanly with plain
   `build-lh5801.sh hello_printf_lcd.c` (it pulls in the C-source stdio
   support this needs -- vprintf.c, printf_large.c, strlen.c, _mulint.c,
   _muluchar.c -- via its own STDIO_SOURCES list, alongside the
   device/lib/lh5801 runtime), and the LCD genuinely renders "Hello,
   1500!" via the ROM's own font routines. Needs the CE-158 extension RAM
   module (or emulator Bus::setExtRam4800Size()) -- printf_large's own
   static data pushes this build past the base 2KB (0x4000-0x47FF).
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
