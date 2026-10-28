/* blackjack.c - Antony F. P. Vickery's 1984 Blackjack for the Sharp
   PC-1500, ported from his BASIC to C for the LH5801 SDCC port.

   The rules are Atlantic City's as of 1984 (Vickery went there for a copy
   of them): dealer stands on soft 17, double down on any first two cards,
   split pairs (one card each on split aces), blackjack pays 3:2, no
   insurance. A basic card count can be shown before each bet.

   Keys during a hand:
     H hit, S or ENTER stick, D double down, X split, F funds, G game
     statistics, T total of bets, C count display on/off, E end the game,
     * this list. BREAK (ON) at any prompt returns to BASIC.

   Build (see support/scripts/build-lh5801.sh) and load it at the bottom
   of user RAM, after reserving it with the NEW the build script prints.
   It needs about 11.5K (10.7K of code), so a PC-1500 with 28K of
   contiguous RAM from 0000H -- still too big for 2K + 8K:
     build-lh5801.sh blackjack.c 00C5
     NEW &xxxx, then load the .bin at 00C5H, then CALL &00C5

   This follows the BASIC program statement by statement -- its labels are
   kept in the comments ("N", "NN", "SX", ...) -- so the game plays the
   same: the same prompts, defaults, displays and pauses, the same deck
   handling (a shoe of 1-4 decks, reshuffled at 85%), the same count
   (+1 for 2-6, -1 for tens and aces) and the same statistics. Its WAIT
   is a global pause mode exactly as in BASIC (wm below), and so is its
   CURSOR-then-PRINT, which prints without clearing the display.

   Money is kept in half dollars: a blackjack pays 3:2, so an odd bet wins
   a half dollar, which BASIC kept as a fraction.

   What differs: RANDOM/RND seed from the ROM's own key-wait counters
   (mixed in at every key), not the clock; BEEP's frequency and duration
   arguments go to the ROM's tone loop as its own half-period and period
   count, which is close to, not exactly, what BEEP does with them; and
   the end-of-game average guards the division by zero BASIC would stop
   on when no hand was played. */

#include <pc1500.h>

typedef long money;               /* half dollars */

#define WENTER 0xFFFF             /* BASIC's bare WAIT: pause for ENTER */
#define FLASH_TICKS 90            /* FOR D=1 TO 20:NEXT D between flashes, ~1/4 s */
#define ACTIVE 2                  /* K(n) of a split hand: 2, 0 or -1 */
#define DONE 0
#define BUST 3

unsigned int wm;                  /* the WAIT in force, in 1/64 s */
char line[40];
unsigned char llen;
char ibuf[12];

char deck[209];                   /* B$(1..R) */
unsigned char a[14];
const char ranks[] = " A23456789TJQK";
unsigned char nd, r, b, mc;

char c[3][32];                    /* C$(1), C$(2): the player's hands */
char e[32];                       /* E$: the dealer's cards shown */
char fh;                          /* F$: the dealer's hole card */
unsigned char t[3], f[3], k[3];
unsigned char u, u1, g, j, h, i, kf, l, q;
unsigned char o, countOn, beepOn, en;
int xx, xb;
money w, x, d1, d2, ww, w1, y, v, z, z1, zz, m[3];
unsigned int s[3], n, mbj, lw[2], ll[2];
unsigned int rs = 0xACE1;

/* --- random numbers: RANDOM / RND -------------------------------------- */

static unsigned int rnd16(void)
{
    rs ^= rs << 7;
    rs ^= rs >> 9;
    rs ^= rs << 8;
    return rs;
}

/* Mixes in how long the ROM's key wait just waited (7B0CH/7B0DH count
   while it waits), so every game shuffles differently. */
static void mix(void)
{
    rs += *(unsigned char *)0x7B0C;
    rs ^= (unsigned int)*(unsigned char *)0x7B0D << 8;
    if (!rs)
        rs = 1;
    rnd16();
}

/* RND cc: 1..cc */
static unsigned char rnd(unsigned char cc)
{
    unsigned char mask = 1;
    unsigned char rv;

    while (mask < cc)
        mask = (mask << 1) | 1;
    do
        rv = rnd16() & mask;
    while (rv >= cc);
    return rv + 1;
}

/* --- keys, sound, WAIT --------------------------------------------------- */

static unsigned char getKey(void)
{
    unsigned char kc = keyWait();

    if (kc == PC1500_KEY_BREAK)
        exit(0);
    mix();
    return kc;
}

static void bp(void)                     /* BEEP 1 */
{
    if (beepOn)
        beep();
}

static void buzz(void)                   /* BEEP 1,100,50 */
{
    if (beepOn)
        beepTone(100, 50);
}

static void pause(void)
{
    if (wm == WENTER) {
        while (getKey() != PC1500_KEY_ENTER)
            ;
    } else if (wm) {
        delayTicks(((wm << 5) + (wm << 3) + (wm << 2) + wm) >> 3);  /* wm * 360/64 */
    }
}

/* --- PRINT ----------------------------------------------------------------- */

static void lclr(void)
{
    llen = 0;
    line[0] = 0;
}

static void laddc(char ch)
{
    if (llen < 39) {
        line[llen++] = ch;
        line[llen] = 0;
    }
}

static void ladd(const char *str)
{
    while (*str)
        laddc(*str++);
}

const long p10[] = {1000000000, 100000000, 10000000, 1000000, 100000,
                    10000, 1000, 100, 10, 1};

static void laddn(long val)              /* STR$ */
{
    unsigned char dd, started = 0, pi;

    if (val < 0) {
        laddc('-');
        val = -val;
    }
    for (pi = 0; pi < 10; pi++) {
        dd = '0';
        while (val >= p10[pi]) {
            val -= p10[pi];
            dd++;
        }
        if (dd != '0' || started || pi == 9) {
            laddc(dd);
            started = 1;
        }
    }
}

static void laddm(money hv)              /* STR$ of an amount */
{
    if (hv < 0) {
        laddc('-');
        hv = -hv;
    }
    laddn(hv >> 1);
    if (hv & 1)
        ladd(".5");
}

static void out(unsigned char col)       /* CURSOR col: PRINT line */
{
    lcdSetCursor(col);
    lcdPutString(line);
    pause();
}

static void pr(void)                     /* PRINT line */
{
    lcdClear();
    out(0);
}

static void say(const char *str)         /* PRINT "..." */
{
    lclr();
    ladd(str);
    pr();
}

static void outStr(unsigned char col, const char *str)
{
    lclr();
    ladd(str);
    out(col);
}

/* --- strings ---------------------------------------------------------------- */

static unsigned char slen(const char *str)
{
    unsigned char len = 0;

    while (str[len])
        len++;
    return len;
}

static void sapp(char *d, const char *src)
{
    d += slen(d);
    while ((*d++ = *src++))
        ;
}

static void sappc(char *d, char ch)
{
    d += slen(d);
    d[0] = ch;
    d[1] = 0;
}

static void eMark(void)                  /* E$ = "> " + E$ */
{
    unsigned char len = slen(e);

    do
        e[len + 2] = e[len];
    while (len--);
    e[0] = '>';
    e[1] = ' ';
}

static unsigned char eCol(void)          /* 26-LEN E$ */
{
    return 26 - slen(e);
}

/* --- INPUT ----------------------------------------------------------------- */

/* INPUT prompt, at col (clearing the display first unless it follows a
   CURSOR). Returns the length typed; 0 keeps the variable's value. */
static unsigned char input(unsigned char col, unsigned char clear, const char *prompt)
{
    unsigned char len;

    if (clear)
        lcdClear();
    lcdSetCursor(col);
    lcdPutString(prompt);
    len = lcdInputLine(ibuf, 10);
    if (len == PC1500_INPUT_BREAK)
        exit(0);
    mix();
    return len;
}

static long val(void)                    /* VAL ibuf */
{
    long vv = 0;
    unsigned char pi;

    for (pi = 0; ibuf[pi] >= '0' && ibuf[pi] <= '9' && pi < 9; pi++)
        vv = (vv << 3) + (vv << 1) + (ibuf[pi] - '0');
    return vv;
}

static money inputMoney(unsigned char col, unsigned char clear, const char *prompt, money dflt)
{
    if (!input(col, clear, prompt))
        return dflt;
    return val() << 1;
}

/* INPUT prompt;L$ ... IF L$="Y" */
static unsigned char inputYes(const char *prompt, unsigned char dflt)
{
    if (!input(0, 1, prompt))
        return dflt;
    return ibuf[0] == 'Y' && ibuf[1] == 0;
}

/* --- cards: "H" ------------------------------------------------------------- */

static void cardValue(char cd)
{
    if (cd == 'A') {
        h = 1;
        xx--;
    } else if (cd == 'T' || cd == 'J' || cd == 'Q' || cd == 'K') {
        h = 10;
        xx--;
    } else {
        h = cd - '0';
        if (h < 7)
            xx++;
    }
}

/* B=B+1, the card added to a hand string */
static void dealTo(char *hand, unsigned char comma)
{
    b++;
    if (comma)
        sappc(hand, ',');
    sappc(hand, deck[b]);
    cardValue(deck[b]);
}

static unsigned char soft(unsigned char hi)   /* T(I)+(F(I)*10) */
{
    return t[hi] + (f[hi] ? 10 : 0);
}

static unsigned char is2021(void)
{
    unsigned char so = soft(i);

    return t[i] == 20 || t[i] == 21 || so == 20 || so == 21;
}

static void reveal(void)                 /* E$=E$+","+F$ */
{
    sappc(e, ',');
    sappc(e, fh);
}

/* --- the displays: "I", "I1", "Z", "CA", "GA", "HE" -------------------- */

static void screen(const char *pmark, unsigned int dwait)
{
    lcdClear();
    wm = 0;
    lclr();
    ladd(c[i]);
    ladd(pmark);
    out(0);
    wm = dwait;
    outStr(eCol(), e);
}

static void screenI(void)
{
    screen("", WENTER);
}

static void screenI1(void)
{
    screen(" ?", 0);
}

static void screenZ(void)
{
    screen("", 1);
}

/* PRINT C$(I) (+ mark), then "> "+E$ at col, pausing for ENTER */
static void lostScreen(const char *pmark, unsigned char col)
{
    lcdClear();
    wm = 0;
    lclr();
    ladd(c[i]);
    ladd(pmark);
    out(0);
    wm = WENTER;
    lclr();
    ladd("> ");
    ladd(e);
    out(col);
}

static void cashLine(void)               /* PRINT "CASH=$";W;" (";W-X;")" */
{
    lclr();
    ladd("CASH=$");
    laddm(w);
    ladd(" (");
    laddm(w - x);
    laddc(')');
    pr();
}

static void showCash(void)               /* "CA" */
{
    wm = 75;
    if (en)
        wm = WENTER;
    cashLine();
}

static void stat2(const char *l1, unsigned int v1, const char *l2, unsigned int v2)
{
    lclr();
    ladd(l1);
    laddn(v1);
    ladd(l2);
    laddn(v2);
}

static void stats(void)                  /* "GA" */
{
    wm = 150;
    if (en)
        wm = WENTER;
    stat2("P-HANDS=", n, ",B-JACKS=", mbj);
    pr();
    stat2("HANDS W=", s[0], ",L=", s[1]);
    ladd(",D=");
    laddn(s[2]);
    pr();
    lclr();
    ladd("MIN=$");
    laddm(d1);
    ladd(",MAX=$");
    laddm(d2);
    pr();
    stat2("LONG WINS=", lw[1], ",LOSSES=", ll[1]);
    pr();
}

const char *const helpLines[] = {
    "C = COUNT ON/OFF TOGGLE", "D = DOUBLE DOWN", "E = END GAME",
    "F = FUNDS DISPLAY", "G = GAME STATISTICS", "H = HIT",
    "S OR ENTER = STICK", "T = TOTAL BETS", "X = SPLIT", "* = COMMAND LIST"};

static void help(void)                   /* "HE" */
{
    unsigned char hl;

    wm = 150;
    for (hl = 0; hl < 10; hl++)
        say(helpLines[hl]);
}

static void totalBets(void)              /* PRINT "TOTAL BETS=$";STR$ ZZ */
{
    lclr();
    ladd("TOTAL BETS=$");
    laddm(zz);
    pr();
}

/* --- "END" ---------------------------------------------------------------- */

/* unsigned 32/16 division, for the one division the game needs */
static unsigned long udiv(unsigned long nn, unsigned int dd)
{
    unsigned long qq = 0, rr = 0;
    unsigned char bit;

    for (bit = 0; bit < 32; bit++) {
        rr = (rr << 1) | (nn >> 31);
        nn <<= 1;
        qq <<= 1;
        if (rr >= dd) {
            rr -= dd;
            qq |= 1;
        }
    }
    return qq;
}

static void endGame(void)
{
    unsigned int hands;
    money diff;
    unsigned long cents;
    char digits[12];
    char sign = '+';
    unsigned char nd2, di;

    en = 1;
    showCash();
    stats();
    totalBets();

    /* USING "+####.##":PRINT "AVG. WIN/LOSS=";((W-X)/(S(0)+S(1)+S(2))) */
    hands = s[0] + s[1] + s[2];
    diff = w - x;
    if (diff < 0) {
        diff = -diff;
        sign = '-';
    }
    /* half dollars * 50 = cents, rounded */
    cents = hands ? udiv(((unsigned long)diff << 5) + ((unsigned long)diff << 4) +
                         ((unsigned long)diff << 1) + (hands >> 1), hands)
                  : 0;
    lclr();
    laddn(cents);
    /* at least three digits, d.dd: "5" -> "005" */
    nd2 = llen < 3 ? 3 : llen;
    for (di = 0; di < nd2; di++)
        digits[di] = di < nd2 - llen ? '0' : line[di - (nd2 - llen)];
    lclr();
    ladd("AVG. WIN/LOSS=");
    for (di = nd2 + 2; di < 8; di++)   /* "+####.##" is 8 wide */
        laddc(' ');
    laddc(sign);
    for (di = 0; di < nd2 - 2; di++)
        laddc(digits[di]);
    laddc('.');
    laddc(digits[nd2 - 2]);
    laddc(digits[nd2 - 1]);
    pr();
    exit(0);
}

/* --- betting: "F" ----------------------------------------------------------- */

static void betweenHands(void)
{
    if (w > ww) {
        lw[0]++;
        ll[0] = 0;
    }
    if (lw[0] > lw[1])
        lw[1] = lw[0];
    if (w < ww) {
        ll[0]++;
        lw[0] = 0;
    }
    if (ll[0] > ll[1])
        ll[1] = ll[0];
    ww = w;
    if (w < d1)
        d1 = w;
    if (w > d2)
        d2 = w;
    if (y > w) {
        wm = WENTER;
        say("GAME OVER - FUNDS < MIN.");
        endGame();
    }
    if (w >= w1) {
        wm = WENTER;
        say("GAME OVER - WIN LIMIT HIT");
        endGame();
    }
F1:
    if (!countOn) {
        z = inputMoney(0, 1, "BET? $", z);
    } else {
        wm = 0;
        lclr();
        ladd("C= ");
        laddn(xx);
        laddc(',');
        laddn((int)mc - (int)b);
        pr();
        wm = 75;
        z = inputMoney(12, 0, " BET? $", z);
    }
    if (z < y) {
        lclr();
        ladd("MINIMUM BET IS $");
        laddm(y);
        pr();
        goto F1;
    }
    if (z > v) {
        lclr();
        ladd("MAXIMUM BET IS $");
        laddm(v);
        pr();
        goto F1;
    }
    if (z > w) {
        lclr();
        laddc('$');
        laddm(z);
        ladd(" BET > $");
        laddm(w);
        ladd(" CASH");
        pr();
        goto F1;
    }
    /* "SGE" */
    w -= z;
    zz += z;
}

/* --- "D": the questions --------------------------------------------------- */

static void setup(void)
{
    nd = 4;
    x = 400;
    w = x;
    d1 = w;
    d2 = w;
    ww = w;
    w1 = 800;
    y = 10;
    v = 1000;
    wm = 75;
    if (inputYes("STANDARD GAME? <Y>/N ", 1)) {
        w -= z;                   /* "SGE" */
        zz += z;
        return;
    }
    input(0, 1, "HOW MANY DECKS? <4> ");
    if (ibuf[0]) {
        long dk = val();
        nd = (dk < 1 || dk > 4) ? 4 : (unsigned char)dk;
    }
    x = inputMoney(0, 1, "CASH ON HAND? <200> $", x);
    w = x;
    d1 = w;
    d2 = w;
    ww = w;
D1:
    w1 = inputMoney(0, 1, "WIN LIMIT? <CASH*2> $", x << 1);
    if (w1 <= x) {
        say("WIN LIMIT NOT > CASH");
        goto D1;
    }
    countOn = inputYes("DISPLAY COUNT? Y/<N> ", 0);
    beepOn = inputYes("BEEP ON? Y/<N> ", 0);
E:
    y = inputMoney(0, 1, "MINIMUM BET? <5> $", y);
    if (y < 2) {
        say("MIN. BET < 1");
        goto E;
    }
    v = inputMoney(0, 1, "MAXIMUM BET? <500> $", v);
    if (v < y) {
        say("MAX. BET < MIN. BET");
        goto E;
    }
    betweenHands();
}

/* --- "A": the shuffle ------------------------------------------------------- */

static void shuffle(void)
{
    unsigned char cc, hh, jj, dd;

    wm = 0;
    say("SHUFFLING");
    for (hh = 1; hh <= 13; hh++)
        a[hh] = nd << 2;
    for (cc = r; cc; cc--) {
        dd = rnd(cc);
        jj = 0;
        for (hh = 1; hh < 13; hh++) {
            jj += a[hh];
            if (jj >= dd)
                break;
        }
        a[hh]--;
        deck[cc] = ranks[hh];
        lclr();
        laddc(' ');               /* PRINT C: a number prints with a blank for its sign */
        laddn(cc);
        ladd("  ");
        out(9);
    }
    lcdClear();
    wm = 0;
    for (jj = 25; jj-- > 0;)
        if (beepOn)
            beepTone(jj ? jj : 1, 100);   /* BEEP 1,J */
    b = 1;
    xx = 0;
    cardValue(deck[1]);
}

/* --- results: "L", "M", "S", "W" -------------------------------------------- */

static void cashAfter(void)              /* K=0:WAIT 25:PRINT "CASH=$"... */
{
    kf = 0;
    wm = 25;
    cashLine();
}

static void playerBlackjack(void)        /* "L" */
{
    unsigned char fl;

    s[0]++;
    w += (z << 1) + (z >> 1);     /* the bet back, and 3:2 */
    sapp(c[i], " <");
    mbj++;
    lcdClear();
    wm = 0;
    outStr(0, c[i]);
    outStr(eCol(), e);
    for (fl = 0; fl < 6; fl++) {
        bp();
        delayTicks(FLASH_TICKS);
        lcdInvert();
    }
    wm = 75;
    cashLine();
}

static void push(void)                   /* "M" */
{
    s[2]++;
    w += z;
    sapp(c[i], " <");
    eMark();
    screenI();
    wm = 25;
    cashLine();
}

static void lose(void)                   /* "S" */
{
    s[1]++;
    eMark();
    buzz();
    screenI();
    cashAfter();
}

static void win(void)                    /* "W" */
{
    s[0]++;
    w += z << 1;
    sapp(c[i], " <");
    bp();
    screenI();
    cashAfter();
}

/* --- "J": the deal -------------------------------------------------------------- */

static void checkDeal(void)
{
    n++;
    if (t[i] == 20 || soft(i) == 20)
        return;
    if (t[i] != 11 || f[i] != 1) {
        screenI1();
        return;
    }
    t[i] = 21;
    kf = 0;
    if (j != 1) {
        playerBlackjack();
        return;
    }
    reveal();
    if (u != 11) {
        playerBlackjack();
        return;
    }
    if (g == 0)
        return;
    u = 21;
    kf = 0;
    push();
}

/* --- "Q": hit ----------------------------------------------------------------- */

static void hit(void)
{
    dealTo(c[i], 1);
    t[i] += h;
    if (h == 1)
        f[i] = 1;
    if (t[i] > 21) {
        lose();
        return;
    }
    if (z1 == 0 && !is2021())
        screenI1();
}

/* --- "U", "V": settling one hand --------------------------------------------- */

static void settle(void)
{
    if (u > 21) {
        win();
        return;
    }
    if (soft(i) > 21)
        f[i] = 0;
    t[i] = soft(i);
    if (t[i] + u == 42) {
        if (slen(e) == 3)
            lose();
        else
            push();
        return;
    }
    if (t[i] > u)
        win();
    else if (t[i] < u)
        lose();
    else
        push();
}

/* --- "T": the dealer plays --------------------------------------------------- */

static void dealerDraw(void)             /* B=B+1:E$=E$+","+B$(B)...:U=U+H */
{
    dealTo(e, 1);
    u += h;
    if (h == 1)
        g = 1;
}

static void dealerPlay(void)
{
    if (u + (g ? 10 : 0) == 21) {
        reveal();
        w += z1;
        lose();
        return;
    }
    u1 = u;
    if (u + (g ? 10 : 0) > 16) {
        reveal();
        u += g ? 10 : 0;
    }
    if (u1 + (g ? 10 : 0) > 16) {
        settle();
        return;
    }
    reveal();
    screenZ();
    for (;;) {                    /* "T1" */
        dealerDraw();
        if (g == 0) {             /* "T2" */
            if (u > 16)
                break;
            screenZ();
            continue;
        }
        u1 = u;
        if (u + 10 < 17) {
            screenZ();
            continue;
        }
        if (u + 10 < 22)
            u += 10;
        if (u1 + 10 < 22 || u > 16)
            break;
        screenZ();
    }
    settle();
}

/* --- "DD": double down ------------------------------------------------------ */

static unsigned char ddRefused(unsigned char needLen)
{
    wm = 75;
    if (slen(c[i]) != needLen) {
        buzz();
        say("DD NOT ALLOWED");
        return 1;
    }
    if (w == 0) {
        buzz();
        say("NO FUNDS FOR A DD BET");
        return 1;
    }
    return 0;
}

static void doubleDown(void)
{
    if (ddRefused(3)) {
        o = 0;
        return;
    }
    z1 = w >= z ? z : w;
    w -= z1;
    z += z1;
    zz += z1;
    hit();
    if (kf == 1)
        dealerPlay();
}

/* --- splitting: "SX", "SH", "SHY", "SD", "SE" ---------------------------- */

static void splitBust(void)
{
    k[i] = BUST;
    buzz();
    s[1]++;
    lostScreen("", 23);
}

static void splitStand(void)             /* "SHY" */
{
    lcdClear();
    wm = 0;
    outStr(0, c[i]);
    wm = 50;
    outStr(25, e);
    k[i] = DONE;
}

static void splitHit(void)               /* "SH" */
{
    dealTo(c[i], 1);
    t[i] += h;
    if (h == 1)
        f[i] = 1;
    if (t[i] < 22) {
        if (is2021())
            splitStand();
        return;
    }
    splitBust();
}

static void splitDouble(void)            /* "SD" */
{
    if (ddRefused(6))
        return;
    m[i] = w >= z ? z : w;
    w -= m[i];
    zz += m[i];
    dealTo(c[i], 1);
    t[i] += h;
    if (h == 1)
        f[i] = 1;
    if (t[i] < 22)
        splitStand();
    else
        splitBust();
}

static void dealerShows(unsigned char col)    /* CLS:CURSOR col:WAIT 1:PRINT E$ */
{
    lcdClear();
    wm = 1;
    outStr(col, e);
}

static void settleSplit(void)            /* "SE" */
{
    unsigned char c1, p = 0, kk;

    for (c1 = 1; c1 <= 2; c1++) {
        i = c1;
        if (k[c1] == BUST)
            goto SE10;
        if (p == 0)
            reveal();
        if (u == 11 && g == 1) {
            kk = 'B';             /* "DB": the dealer's blackjack */
            goto SE9;
        }
        if (p == 1 || u + (g ? 10 : 0) > 16)
            goto SE1;
        dealerShows(23);
        for (;;) {                /* "SEC" */
            p = 1;
            dealerDraw();
            if (g == 0 && u > 16)
                break;
            if (g != 0 || u >= 17) {
                if (u + 10 >= 17) {
                    if (u + 10 < 22) {
                        u += 10;
                        break;
                    }
                    if (u > 16)
                        break;
                }
            }
            dealerShows(eCol());
        }
SE1:
        p = 1;
        if (u > 21) {
            u = 0;
            g = 0;
        }
        if (soft(i) > 21)
            f[i] = 0;
        t[i] = soft(i);
        if (u + (g ? 10 : 0) > 21)
            g = 0;
        u += g ? 10 : 0;
        kk = t[i] > u ? 'P' : t[i] < u ? 'D' : 'O';
SE9:
        if (kk == 'P') {          /* "PW" */
            s[0]++;
            w += (z + m[i]) << 1;
            sapp(c[i], " <");
            bp();
            screenI();
        } else if (kk == 'B') {   /* "DB" */
            s[1]++;
            w += m[i];
            buzz();
            if (q == 1)
                w += z >> 1;
            lostScreen("", 21);
        } else if (kk == 'O') {   /* "SO" */
            s[2]++;
            w += z + m[i];
            lostScreen(" <", 24 - slen(e));
        } else {                  /* "DW" */
            s[1]++;
            buzz();
            lostScreen("", 24 - slen(e));
        }
SE10:
        k[i] = BUST;
    }
    wm = 25;
    cashLine();
}

static unsigned char splitKeyOk(void)
{
    return o == 'H' || o == PC1500_KEY_ENTER || o == 'S' || o == 'D' || o == 'F' ||
           o == 'G' || o == 'T';
}

static void split(void)                  /* "SX" */
{
    char first;

    wm = 75;
    xb = xx;
    cardValue(deck[b - 2]);
    xx = xb;
    if (t[i] != h << 1) {
        buzz();
        say("SPLIT NOT ALLOWED");
        o = 0;
        return;
    }
    if (w < z) {
        say("NO FUNDS FOR A SPLIT");
        o = 0;
        return;
    }
    w -= z;
    zz += z;
    l = 1;
    k[1] = ACTIVE;
    k[2] = ACTIVE;
    m[1] = 0;
    m[2] = 0;
    q = 0;
    first = c[1][0];
    c[2][0] = 'X';
    c[2][1] = '2';
    c[2][2] = ' ';
    c[2][3] = first;
    c[2][4] = 0;
    c[1][0] = 'X';
    c[1][1] = '1';
    c[1][2] = ' ';
    c[1][3] = first;
    c[1][4] = 0;
    t[1] = h;
    t[2] = h;
    f[2] = f[1];
    if (t[1] == 1) {              /* split aces: one card each */
        q = 1;
        i = 1;
        splitHit();
        k[1] = DONE;
        i = 2;
        splitHit();
        k[2] = DONE;
        goto SE;
    }
    dealTo(c[1], 1);
    t[1] += h;
    if (h == 1)
        f[1] = 1;
    if (soft(1) >= 20) {
        splitStand();
        i = 2;
        k[1] = DONE;
        dealTo(c[2], 1);
        t[2] += h;
        if (h == 1)
            f[2] = 1;
        if (soft(2) > 19)
            k[2] = DONE;
        if (k[1] != ACTIVE && k[2] != ACTIVE)
            goto SE;
    }
    screenI1();                   /* "SB" */
    o = 0;
    for (;;) {                    /* "SQ" */
        o = getKey();
        if (!splitKeyOk()) {
            wm = 75;
            buzz();
            say("C,F,D,G,H,S,T ONLY");
            screenI1();
            continue;
        }
        if (o == PC1500_KEY_ENTER || o == 'S')    /* "SA" */
            k[i] = DONE;
        if (o == 'D')
            splitDouble();
        if (o == 'H')
            splitHit();
        if (o == 'F')
            showCash();
        if (o == 'T') {
            wm = 75;
            totalBets();
        }
        if (o == 'G')
            stats();
        if (i == 1 && k[1] != ACTIVE) {
            i = 2;
            splitHit();
        }
        if (k[1] != ACTIVE && k[2] != ACTIVE) {
            o = 'X';
            goto SE;
        }
        screenI1();
    }
SE:
    settleSplit();
}

/* --- the game ------------------------------------------------------------------ */

static unsigned char keyOk(void)         /* line 450 */
{
    return o == 'H' || o == PC1500_KEY_ENTER || o == 'S' || o == 'D' || o == 'X' ||
           o == 'E' || o == 'F' || o == 'G' || o == 'T' || o == 'C' || o == '*';
}

/* T(I)<10 AND F(I)=0 AND T(I)<>H*2: too low to stick, so hit */
static unsigned char mustHit(void)
{
    return t[i] < 10 && f[i] == 0 && t[i] != h << 1;
}

int main(void)
{
    unsigned char fl;

    /* "B-J" */
    wm = 0;
    say("  BLACKJACK - AFPV 1984");
    for (fl = 0; fl < 11; fl++) {
        delayTicks(FLASH_TICKS);
        lcdInvert();
    }
    z = 20;
    setup();
    r = nd * 52;
G:
    shuffle();
    wm = WENTER;
N:
    i = 1;
    kf = 1;
    l = 0;
    f[1] = 0;
    f[2] = 0;
    g = 0;
    b++;
    mc = nd * 44;                 /* INT(R*.85) */
    if (b > mc) {
        say("NEW DECK REQUIRED");
        goto G;
    }
    if (mc - b < 16 && beepOn)
        beepTone(25, 1200);       /* BEEP 1,25,1200: near the cut card */
    c[1][0] = deck[b];
    c[1][1] = 0;
    sappc(c[1], ',');
    cardValue(deck[b]);
    t[1] = h;
    if (h == 1)
        f[1] = 1;
    dealTo(c[1], 0);
    t[1] += h;
    if (h == 1)
        f[1] = 1;
    e[0] = 0;
    dealTo(e, 0);
    u = h;
    if (h == 1)
        g = 1;
    j = h == 1 || h == 10;
    b++;
    fh = deck[b];
    cardValue(fh);
    u += h;
    if (h == 1)
        g = 1;
    checkDeal();
    if (kf == 0) {
        betweenHands();
        goto N;
    }
NN:
    o = 0;
    z1 = 0;
    if (t[i] == 20 || soft(i) == 20) {
        o = 'S';
        goto P;
    }
    xb = xx;
    cardValue(deck[b - 2]);
    xx = xb;
    if (mustHit()) {
        o = 'H';
        goto P;
    }
O:
    o = getKey();
    if (!keyOk()) {
        wm = 75;
        buzz();
        say("C,D,E,F,G,H,S,T,X,* ONLY");
        screenI1();
        goto O;
    }
P:
    if (o == 'H')
        hit();
    if (o == 'X' && l) {
        wm = WENTER;
        say("SPLIT NOT ALLOWED - X");
        goto P1;
    }
    if (kf == 0) {
        betweenHands();
        goto N;
    }
    if (is2021())
        o = 'S';
    if (mustHit())
        o = 'H';
    if (o == 'H')
        goto NN;
    if (o == PC1500_KEY_ENTER || o == 'S') {
        dealerPlay();
        betweenHands();
        goto N;
    }
    if (o == 'D') {
        doubleDown();
        if (o == 'D') {
            z -= z1;
            betweenHands();
            goto N;
        }
    }
    if (o == 'X') {
        split();
        if (o == 'X') {
            betweenHands();
            goto N;
        }
    }
    if (o == 'F')
        showCash();
    if (o == 'T') {
        wm = 75;
        totalBets();
    }
    if (o == 'G')
        stats();
    if (o == 'C')
        countOn = !countOn;
    if (o == 'E') {
        w += z;
        n--;
        endGame();
    }
    if (o == '*')
        help();
P1:
    screenI1();
    goto O;
}
