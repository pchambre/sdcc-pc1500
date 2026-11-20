/* lcdPutChar()'s ASCII: ' [ and ], which the ROM's font lacks, are drawn
   by the library (lcd_draw.asm); PC1500_CHAR_BOX/SQRT/PI give the ROM's
   own 27H, 5BH and 5DH. chars.script reads the display: the runner knows
   the library's three shapes, and reads the ROM's three as '~' (it
   learns its font from BASIC PRINTs, which can't type them). */
#include "check.h"

int main(void)
{
    lcdClear();
    lcdPutString("IT'S [OK]");
    CHECK(1, PC1500_GCU == 9 * 6);          /* a cell each, as any character */
    keyWait();

    lcdClear();
    lcdPutChar('X');
    lcdPutChar(PC1500_CHAR_BOX);
    lcdPutChar(PC1500_CHAR_SQRT);
    lcdPutChar(PC1500_CHAR_PI);
    lcdPutString("Y'");
    CHECK(2, PC1500_GCU == 6 * 6);
    keyWait();

    lcdSetCursor(25);
    lcdPutString("]]");                     /* only one fits, as for the ROM's */
    CHECK(3, PC1500_GCU == 156);
    pass();
    return 0;
}
