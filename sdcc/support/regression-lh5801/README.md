# LH5801 regression tests

Self-checking C programs run on an emulated PC-1500, plus the Blackjack
example played with stacked decks. Each test shows `OK` or `FAIL n` (the
first failed check, see `tests/check.h`); tests that need keys have a
`tests/<name>.script` for [pc1500runner](https://github.com/pchambre/pc1500emu)
(`tools/runner` in pc1500emu, which documents the script language).

```sh
export PC1500RUNNER=/mnt/c/Users/paulc/pc1500emu/build/tools/runner/Release/pc1500runner.exe
export PC1500ROM=/mnt/c/Users/paulc/Documents/PC1500/ROM1.BIN
support/regression-lh5801/run.sh            # everything
support/regression-lh5801/run.sh calls      # one test
```

Run from WSL (the compiler builds there); a Windows `pc1500runner.exe` works
too. `SDCCFLAGS` passes extra compiler options to every build, the runtime
library's included -- e.g. `SDCCFLAGS=--oldralloc run.sh` runs the suite
without register allocation (every value in memory), the fallback. Every test is built at 00C5 and run with 28K of contiguous RAM
(16K at 0000H + 2K + 10K at 4800H), after `NEW` reserves its RAM.

| test | covers |
|---|---|
| arith8, arith16, arith32 | + - * / % compare bitwise shift, signed and unsigned, boundaries; widening and narrowing |
| memory | arrays, 2-D arrays, pointers, structs, string.h (incl. the TIN/SIN/LIN/CIN memcpy, memset, strcpy, strlen), calls through pointers (reentrant) |
| calls | mixed parameter sizes, 1/2/4-byte returns, nested calls, sprintf varargs |
| control | loops, break/continue, goto, ?:, && and \|\| short-circuit, switch |
| elements | in-place `s[k] op= c` and `x op= c`: carries, borrows, masks, an unchainable constant, post-increments that must not fuse |
| overlay | overlaid locals survive call chains, calls as arguments, calls through pointers (taken by name or from an initializer) and recursion |
| init | zeroed and initialized globals, static locals, const tables, rand()'s seed |
| exitret | exit() from inside nested calls returns to BASIC |
| library | keyWait/keyScan, lcdInputLine, lcdSetCursor, the 26-cell limit, beep, lcdInvert |
| blackjack | the example game: 3:2 blackjack, split, double, push, end statistics |

`src/lh5801/test_smoke.py` separately checks the exact linked bytes of a
minimal program (startup included).

## Size baseline

Image bytes at the start of the register-allocator work (2026-10-27):

| test | image | RAM |
|---|---:|---:|
| arith8 | 3016 | 3317 |
| arith16 | 4487 | 4861 |
| arith32 | 6527 | 7093 |
| calls | 7912 | 8666 |
| control | 2303 | 2518 |
| exitret | 1416 | 1572 |
| init | 2425 | 2708 |
| library | 2048 | 2273 |
| memory | 5019 | 6384 |
| blackjack | 19140 | 20722 |
