# Colossal Cave Adventure for the PC-1500

Crowther and Woods' original 350-point *Adventure*, for a PC-1500 with
banked RAM: the internal expansion card, or a PC-1500A (or PC-1501) with
a CE-163F. The game is too big for the PC-1500's own memory: its text
alone is 41K. Five 16K banks hold the text, the tables and most of the
code, and the compiler switches between them on every `__banked` call.

It is a port of Jonathan Wodder's C translation of the PDP-10 Fortran,
[jwodder/advent350](https://github.com/jwodder/advent350) (commit a2f7ab0),
which is in the public domain; so is this port. `orig/` holds the data files
from that translation unchanged. The game logic is jwodder's, function for
function, and checked against it (see Tests).

## Playing

The text comes one line of 26 characters at a time:

| key | at a line of text | at the `>` prompt |
|---|---|---|
| ENTER, ▼, SPACE | the next line | ENTER: send the command |
| ▲ | back through the last 10 lines (▼ forward again) | the same |
| a letter | starts the command (type-ahead) | typed |
| ◀ / CL | | delete a character / the whole command |
| BREAK | save the game and go back to BASIC | the same |

Commands are one or two words, and only the first five letters of each
count, as in the original (`NE` for northeast, `GET LAMP`, `XYZZY`).
`SUSPEND` saves the game and goes back to BASIC, and so does BREAK. The
next `CALL &40C5` asks whether to resume it. `RESUME` loads the saved game
during play. The game is kept in bank 0, which keeps it while the PC-1500
is off. Loading the program again clears it.

With the SD/BLE expansion module (the card has its ROM; a PC-1500A can
have the dongle), the game is built to work with what it adds
(build-lh5801.sh `--expansion`): its Bluetooth keyboard (MCONF BLKBD)
plays it, title screen included, and the host app's Screen pane (MCONF
BLDISP) shows it as it shows BASIC -- each line of text, the command as
it's typed, and the command entered.

## Building and loading

```sh
gen/gen.sh            # only if orig/ or gen/gen.py changed
mkdir build && cd build && ../make.sh            # the card
mkdir build && cd build && ../make.sh --ce163f   # a PC-1500A with a CE-163F
```

Both build the fixed part at 40C5H and banks 0-4, and write `LOAD.TXT`,
the steps to load them.

**The card:** `MAIN.BIN` is the SD-card file with the fixed code and all
five banks:

```
NEW &6EFF
SDLOAD M "MAIN.BIN"
```

loads it all and starts it (the `NEW` address is the one `LOAD.TXT`
gives). The banks' code replaces any BASIC programs kept in banks 0-4 with
`BANK`.

**A PC-1500A with a CE-163F** (`--ce163f`): banks 0-4 of the CE-163F's
RAM, switched at 6800H. A bank's code starts at 0113H: its vendor's
routines keep the first 112H bytes of every bank. `LOAD.TXT` has the
`POKE &680n,0` and `CLOAD M` for each bank, then `NEW &567E` and the
fixed part. A PC-1501 is the same machine.

Either way `CALL &40C5` starts it again later. Its line buffers use the
system RAM of BASIC's fixed variables (7650H-7683H, 78C0H-79C3H), so A-Z
and A$-Z$ are overwritten.

## Where things are

| | file | |
|---|---|---|
| fixed | `start.c` | `main()` |
| | `savegame.c` | copying the saved game to and from bank 0 (it switches the bank) |
| | `advdata.c` | small tables (generated) |
| fixed on the card, bank 4 on a CE-163F | `main.c` | `turn()`, the helpers every bank calls, the variables (fixed in both) |
| | `io.c` | the display, the keyboard |
| bank 0 | `verbs.c`, `vocab.c` | the verbs; the vocabulary and its table; the saved game (`save.asm`) |
| banks 1, 2 | `text_b1.c`, `text_b2.c` | text (generated) |
| bank 3 | `text_b3.c`, `turn.c` | text; the bigger parts of `turn()` |
| bank 4 | `motion.c` | moving, the dwarves, death, the score, and the travel table |

On the card, main.c and io.c are fixed, because every bank switch wakes
the card's MCU. A PC-1500A has only 4000H-57FFH of fixed RAM besides the
system's, so there they go in bank 4 (`ADV_CE163F`, advent.h's
`BANKED_CE`); a CE-163F switches with one store. Every variable stays
fixed in both, since every bank reads them.

`gen/gen.py` makes the banks' data from `orig/`. It joins the original's
70-column lines into paragraphs for word-wrapping, packs the text into the
banks (each bank has its own index and printer, and a bank's text fits
either device), packs the travel table (141 x 13 x 11 ints in the
original, 3K here) and the vocabulary. A bank's data can only be read by
code in the same bank, so each table sits with the code that reads it.

Space left:
- the card: about 260 bytes of fixed RAM below 7000H; 4.0K in bank 0
  (besides the 1200-byte save area), 1.0K in bank 3, 7.5K in bank 4;
- a CE-163F: about 390 bytes below 5800H, and the PC-1500A's 1K at 7C01H
  unused; 2.8K in bank 0, 0.7K in bank 3, 0.6K in bank 4, and banks 5-7.

The CPU stack runs on BASIC's, from about 7842H down. Over the
walkthrough it went no lower than 7816H on the card. On a CE-163F, where
the banked calls are many more, it reached 77FEH -- BASIC's fixed string
variables (7750H-77FFH), which the game gives up anyway; the LCD's
memory, below them, is 174 bytes further down.

## Tests

- `host.sh ORIG_DIR [commands [seed]]` builds the port for a PC (`-DHOST`,
  with `io_host.c`) and jwodder's original (from a checkout of its `c/`
  directory, with `SEEDABLE_RNG`), runs both on the same commands and seed,
  and compares their output with whitespace squeezed. `gen/fuzz.py` makes
  random commands from the game's vocabulary. 16 seeds x 5000 commands
  gave the same output.
- `test/lcdtest.sh BUILD_DIR commands [seed]` checks a PC-1500 build, the
  card's or the CE-163F's (as its `LOAD.TXT` says). It builds the same
  code, `io.c` included, for a PC, with `test/host_lcd.c` standing in for
  the display and keyboard. That logs every display the program waits at
  and the key pressed. Then it plays the same keys on pc1500emu's emulated
  PC-1500 with the card, or PC-1500A with a CE-163F (`pc1500runner`, the
  seed set through the map), checking each display. The walkthrough in
  [Quuxplusone/Advent](https://github.com/Quuxplusone/Advent)'s
  `adventure-family-tree/walkthroughs/WOOD0350.txt` (1368 displays)
  matched on both, and three random games (42,000 displays) on the card.
