# Adventure for a PC-1500A or PC-1501 with a CE-163F

Prebuilt pieces of `../make.sh --ce163f`, for loading without the
toolchain: the 350-point *Adventure* (see `../README.md` for playing it),
built for a PC-1500A or PC-1501 (the same machine, sold in Japan) with a
CE-163F in its memory slot.

The game's code and text are in RAM banks 0-4 of the CE-163F, and its fixed
part (main(), the saved game's copying, every variable) at 40C5H-567DH of
the PC-1500A's own RAM. A bank's code starts at 0113H, past the 112H bytes
the CE-163F's own routines keep in every bank.

| file | what | load at |
|---|---|---|
| `main-40C5-pc1500.bin` | the fixed part | 40C5H |
| `main-40C5-pc1500-b0.bin` ... `-b4.bin` | banks 0-4 (an FFH, then the code) | 0112H of that bank |
| `MAIN.BIN`, `MAINB0.BIN` ... `MAINB4.BIN` | the same with the expansion module's 4-byte SD header, for `SDLOAD M` | |
| `LOAD.TXT` | the steps, as the build wrote them | |

## Loading

Banks 0-4 are overwritten: any BASIC program kept in them is lost. Load the
banks first, while BASIC's program is still in 0000H-3FFFH, each with its
bank switched in, then the fixed part:

```
H=PEEK &E2
POKE &6800,0
CLOAD M "mainB0";&0112
  (likewise POKE &6801,0 ... &6804,0 for banks 1-4)
POKE &6800+H,0
NEW &567E
CLOAD M "main";&40C5
CALL &40C5
```

`NEW` is typed in PRO mode, the rest in RUN mode. `CALL &40C5` starts the
game again later. With the SD/BLE expansion module attached, `SDLOAD M
"MAINB0.BIN"` (and so on) can stand in for each `CLOAD M`; the bank files'
headers don't start anything.

## Notes

- The saved game (`SUSPEND`, or BREAK during play) is kept in bank 0, after
  the code, and the next `CALL &40C5` offers to resume it.
- The game uses BASIC's fixed variables' system RAM for its line buffers:
  A-Z and A$-Z$ are overwritten.
- With the expansion module, its Bluetooth keyboard (MCONF BLKBD) plays the
  game and the host app's Screen pane (MCONF BLDISP) shows it.
- Tested in pc1500emu on an emulated PC-1500A with a CE-163F: the WOOD0350
  walkthrough's 1368 displays matched a PC build of the same code. Not yet
  run on a real CE-163F.
