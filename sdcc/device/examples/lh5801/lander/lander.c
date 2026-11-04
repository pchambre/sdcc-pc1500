/* lander.c - a lunar-lander game for the Sharp PC-1500, written for the
   LH5801 SDCC port as a second real program to measure the compiler
   against (with Blackjack and the EDIT example). Unlike Blackjack it
   leans on float arithmetic and sqrtf(), with sprintf() for the display.
   SDCC's printf has no %f on this port (as on every port but ds390), so
   values are shown in tenths through tenths() -- what an SDCC program on
   any small target does.

   Pick a world, then set the engine's burn (kg of fuel a second, 0-20)
   once a second until you are down. The display shows height (m),
   descent speed (m/s, + is down) and fuel left (kg):
     H1000 V 40.0 F300 B?
   ENTER alone keeps the last burn; BREAK (ON) returns to BASIC.
   Touchdown under 1 m/s is perfect, under 3 good, under 6 hard (but you
   walk away); faster is a crater. The score is the fuel you have left,
   doubled for a perfect landing, halved for a hard one.

   The physics runs in ten 0.1 s steps a second: thrust is burn x exhaust
   speed over the lander's mass, which falls as fuel burns. Touchdown is
   found exactly within the last step, by solving its equation of motion.

   Build (support/scripts/build-lh5801.sh), reserve the RAM with the NEW
   the build prints, load the .bin and CALL it:
     build-lh5801.sh lander.c 00C5      (28K of RAM from 0000H)

   Paul Chambre, 2026 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pc1500.h>

#define DRY_MASS 900.0f      /* kg, without fuel */
#define EXHAUST 450.0f       /* m/s */
#define MAX_BURN 20
#define STEPS 10             /* a second, in steps */

struct world {
    const char *name;
    float g;                 /* m/s^2 */
    float height;            /* start, m */
    float speed;             /* start, m/s down */
    float fuel;              /* kg */
};

static const struct world worlds[] = {
    {"MOON", 1.62f, 1000.0f, 40.0f, 300.0f},
    {"MARS", 3.71f, 1500.0f, 60.0f, 600.0f},
    {"IO",   1.80f,  600.0f, 25.0f, 150.0f},
};
#define NWORLDS (sizeof worlds / sizeof worlds[0])

/* the last landing, for the test script (support/regression-lh5801) */
unsigned char verdict;       /* 0 none, 1 perfect, 2 good, 3 hard, 4 crash */
int score;
int best;
unsigned char seconds;

static float height, speed, fuel;
static char line[32];
static char input[4];
static char num[12];

/* v in tenths, rounded: "-12.3" -- in num, for a "%5s" (within +-3000) */
static const char *tenths(float v)
{
    char *p = num;
    int t;

    if (v > 3000.0f)
        v = 3000.0f;
    else if (v < -3000.0f)
        v = -3000.0f;
    t = (int)(v * 10.0f + (v < 0.0f ? -0.5f : 0.5f));
    if (t < 0) {
        *p++ = '-';
        t = -t;
    }
    sprintf(p, "%d.%d", t / 10, t % 10);
    return num;
}

int putchar(int c)
{
    if (c != '\n')
        lcdPutChar((unsigned char)c);
    return c;
}

static void show(void)
{
    lcdClear();
    lcdPutString(line);
}

/* shows line, waits for a key; BREAK ends the program */
static unsigned char pause(void)
{
    unsigned char k;

    show();
    k = keyWait();
    if (k == PC1500_KEY_BREAK)
        exit(0);
    return k;
}

/* asks at the end of line: a number up to `max`, or `keep` for ENTER */
static unsigned char ask(unsigned char keep, unsigned char max)
{
    unsigned char n, v;

    for (;;) {
        show();
        lcdSetCursor((unsigned char)strlen(line));
        n = lcdInputLine(input, 2);
        if (n == PC1500_INPUT_BREAK)
            exit(0);
        if (n == 0)
            return keep;
        v = (unsigned char)atoi(input);
        if (v <= max && input[0] >= '0' && input[0] <= '9')
            return v;
        beep();
    }
}

/* One second at `burn`. Returns 1 at touchdown, with speed the speed
   at the moment the height reached 0. */
static unsigned char second(const struct world *w, unsigned char burn)
{
    unsigned char i;
    const float h = 1.0f / STEPS;

    for (i = 0; i < STEPS; i++) {
        float rate = burn;
        float a, h0, v0;

        if (fuel < rate * h)
            rate = fuel / h;
        a = w->g - rate * EXHAUST / (DRY_MASS + fuel);
        h0 = height;
        v0 = speed;
        speed += a * h;
        height -= v0 * h + 0.5f * a * h * h;
        fuel -= rate * h;
        if (fuel < 0.0f)
            fuel = 0.0f;
        if (height <= 0.0f) {
            /* h0 - v0 t - a t^2 / 2 = 0 within this step */
            float t;

            if (a > -0.001f && a < 0.001f)
                t = h0 / v0;
            else
                t = (-v0 + sqrtf(v0 * v0 + 2.0f * a * h0)) / a;
            speed = v0 + a * t;
            height = 0.0f;
            return 1;
        }
    }
    return 0;
}

static void land(const struct world *w)
{
    unsigned char burn = 0;

    height = w->height;
    speed = w->speed;
    fuel = w->fuel;
    seconds = 0;
    for (;;) {
        sprintf(line, "H%4d V%5s F%3d B?", (int)(height + 0.5f), tenths(speed), (int)(fuel + 0.5f));
        if (fuel > 0.0f)
            burn = ask(burn, MAX_BURN);
        else
            burn = 0;
        seconds++;
        if (second(w, burn))
            break;
        if (height > 9999.0f) {      /* gone back to space */
            sprintf(line, "LOST IN ORBIT AFTER %us", seconds);
            verdict = 4;
            score = 0;
            pause();
            return;
        }
    }

    if (speed < 1.0f) {
        verdict = 1;
        score = (int)(fuel * 2.0f);
    } else if (speed < 3.0f) {
        verdict = 2;
        score = (int)fuel;
    } else if (speed < 6.0f) {
        verdict = 3;
        score = (int)(fuel / 2.0f);
    } else {
        verdict = 4;
        score = 0;
    }
    {
        static const char *const words[] = {"", "PERFECT", "GOOD", "HARD", "CRASH"};

        sprintf(line, "%s %s M/S %us", words[verdict], tenths(speed), seconds);
    }
    pause();
    if (verdict == 4)
        sprintf(line, "CRATER %s M DEEP", tenths(speed * speed / 100.0f));
    else
        sprintf(line, "SCORE %d  BEST %d", score, score > best ? score : best);
    if (score > best)
        best = score;
    pause();
}

int main(void)
{
    for (;;) {
        unsigned char k, i;
        char *p = line;

        for (i = 0; i < NWORLDS; i++)
            p += sprintf(p, "%u%s ", i + 1, worlds[i].name);
        sprintf(p, "?");
        k = pause();
        if (k < '1' || k >= '1' + NWORLDS) {
            beep();
            continue;
        }
        land(&worlds[k - '1']);
        sprintf(line, "AGAIN (Y/N)?");
        if (pause() == 'N')
            return 0;
    }
}
