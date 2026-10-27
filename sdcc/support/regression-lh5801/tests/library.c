/* The PC-1500 library: keys, line input, cursor, the 26-cell limit,
   buzzer and display reverse. library.script types the keys. */
#include <string.h>
#include "check.h"

char buf[12];
unsigned char codes[4];

int main(void)
{
    unsigned char i, n;

    lcdClear();
    lcdPutString("KEYS?");
    for (i = 0; i < 4; i++)
        codes[i] = keyWait();
    CHECK(1, codes[0] == 'A' && codes[1] == PC1500_KEY_ENTER);
    CHECK(2, codes[2] == PC1500_KEY_CL && codes[3] == '5');
    /* keyWait() returns on the press; keyScan() sees the key until it's
       released, then nothing */
    for (n = 0; keyScan() && n < 200; n++)
        idleTick();
    CHECK(3, keyScan() == 0);

    lcdClear();
    lcdSetCursor(3);
    CHECK(4, PC1500_GCU == 18);
    lcdPutString("IN?");
    n = lcdInputLine(buf, 8);               /* the script types 12, CL, 42 */
    CHECK(5, n == 2 && !strcmp(buf, "42"));

    lcdSetCursor(25);
    lcdPutString("XYZ");                    /* only X fits */
    CHECK(6, PC1500_GCU == 156);

    beep();
    beepTone(40, 50);
    beepTone(10, 0);
    lcdInvert();
    lcdInvert();
    pass();
    return 0;
}
