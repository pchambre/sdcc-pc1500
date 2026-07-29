# sdcc-pc1500

A fork of [SDCC](http://sdcc.sourceforge.net/) (Small Device C Compiler)
adding a from-scratch backend for the **Sharp LH5801** CPU, targeting the
**Sharp PC-1500** pocket computer.

Everything specific to this fork lives under [`sdcc/`](sdcc/), which is
otherwise a normal SDCC checkout — see [`sdcc/doc/README.txt`](sdcc/doc/README.txt)
for upstream SDCC's own documentation. This README covers only what's
different here.

## What's added

- **`sdcc/src/lh5801/`** — the LH5801 compiler backend itself. Deliberately
  minimal ("phase-1"): no register allocation, every operand lives at a
  fixed, directly-addressed memory location. See `gen.c`'s own header
  comment for the design rationale.
- **`sdcc/sdas/aslh5801/`** — the LH5801 assembler, built on the shared
  `sdas` framework used by SDCC's other targets.
- **`sdcc/device/lib/lh5801/`** and **`sdcc/device/include/lh5801/`** — a
  small runtime library, and its header (`pc1500.h`), giving C programs
  access to real PC-1500 hardware and ROM functionality: LCD output, the
  ON key, power-conscious idle/halt, cassette I/O, and ROM-native
  numeric routines (the PC-1500's own floating-point format and math
  functions).
- **`sdcc/support/scripts/build-lh5801.sh`** — a wrapper script that
  compiles, assembles, and links a C source file into a padding-free flat
  binary ready to load onto a real PC-1500 (or the emulator below) at a
  chosen address.

## Design principle: never duplicate ROM functionality

Every PC-1500 ROM/hardware wrapper in `device/lib/lh5801/` calls into the
real ROM's own routines (LCD character printing, cassette I/O, floating-
point math) rather than reimplementing them. A real PC-1500 always has
this ROM present, so there's no reason to spend precious code space
duplicating it — and it keeps generated binaries small. Each `.asm` file
documents exactly which ROM address or vector-table entry it calls, and
how that was confirmed (either from primary documentation or by direct
disassembly and empirical testing against a real ROM dump).

## Building

```sh
./configure
make
```

Then, to compile a C program for the PC-1500:

```sh
sdcc/support/scripts/build-lh5801.sh myprogram.c [load_address_hex]
```

See the script's own header comment for details (default load address,
output naming, and hardware caveats it warns about).

## Status

Early-stage. Core language features, a real ROM-calling standard library
subset, and a growing PC-1500-native numeric library all work and are
tested against real ROM images. C `float`/`double` support is in
progress. See `sdcc/src/lh5801/gen.c` and recent commit history for the
current state.

## Companion project

[**pc1500emu**](https://github.com/pchambre/pc1500emu) is a from-scratch
PC-1500/LH5801 emulator built alongside this compiler port, used to
verify every ROM call and hardware interaction empirically against real
ROM dumps rather than by assumption.

## License

Unchanged from upstream SDCC: GPL, with the usual linking exception for
compiled output (see `sdcc/COPYING` and the license header in each
library source file).
