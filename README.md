# sdcc-pc1500

A fork of [SDCC](http://sdcc.sourceforge.net/) (Small Device C Compiler)
adding a from-scratch backend for the **Sharp LH5801** CPU, enabling C
development for the **Sharp PC-1500** and its US/European rebadge, the
**TRS-80 PC-2** — both built around the identical LH5801 CPU and ROM (only
the case badging differs).

Forked from **SDCC 4.6.2** (a post-4.6.0 development snapshot). Everything
specific to this fork lives under [`sdcc/`](sdcc/), which is otherwise a
normal SDCC checkout — see [`sdcc/doc/README.txt`](sdcc/doc/README.txt)
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

## Changes to core (non-LH5801) SDCC

Most of this fork is new, self-contained code under the `lh5801`-named
paths above. A handful of shared files used by every SDCC target needed
small changes too:

- **`src/SDCCmain.c`**, **`src/port.h`** — the standard `TARGET_ID_LH5801`
  registration (port table entry, `TARGET_IS_LH5801` macro, extern
  declaration) every new SDCC backend adds; no behavior change for other
  targets.
- **`src/SDCCglue.c`** — added `TARGET_IS_LH5801` cases to the
  `__sdcc_program_startup` stub's mnemonic selection (this code hardcoded
  mcs51-style `lcall`/`ljmp`/`sjmp` with no fallback for an unrecognized
  target). Dead code for this port in practice (LH5801's own
  `genInitStartup` jumps straight to `_main`), but it still has to
  assemble.
- **`src/SDCCsymt.c`** — added `TARGET_IS_LH5801` to the list of
  no-native-multiply, byte-oriented targets when registering the compiler's
  built-in `_muluchar`/`_mulint`/etc. helper function types. Without this,
  `_muluchar` got registered as returning `unsigned char` instead of its
  actual `unsigned int`, silently conflicting with
  `device/lib/_muluchar.c`'s real declaration (LH5801 has no multiply
  instruction at all, so it belongs with the other targets already on this
  list).
- **`device/include/sdcc-lib.h`**, **`device/include/stdarg.h`** — the
  standard per-target dispatch every port adds: including
  `asm/lh5801/features.h`, and adding `__SDCC_lh5801` to the target list
  that gets the generic `va_list`-as-`unsigned char*` varargs
  implementation (needed for `printf`-family functions to compile at all).
- **`device/include/stdbit.h`** — a genuine, previously-latent bug fix, not
  just registration: `__STDC_ENDIAN_NATIVE__` only special-cased
  `hc08`/`s08`/`stm8` as big-endian, defaulting every other port (LH5801
  included) to little-endian. This backend is unambiguously big-endian
  (byte 0 = most significant byte, throughout), and portable library code
  that branches on this macro to pick a byte-order-dependent union layout
  (e.g. `device/lib/_mullong.c`'s multiply) silently computed wrong answers
  without it.

Everything else — the LH5801 code generator, the `sdaslh5801` assembler,
the PC-1500 runtime library, and the build-system wiring every new port
needs (`configure.ac`, `Makefile.in`, etc.) — is additive and lives
entirely under `lh5801`-specific paths.

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

Early-stage. Core language features and a real ROM-calling standard
library subset work and are tested against real ROM images. Two numeric
paths are being developed in parallel:

- A **PC-1500-native numeric library** (`pc1500Add`/`pc1500Sqrt`/etc. in
  `pc1500.h`) calling the ROM's own 8-byte BCD floating-point routines
  directly — complete and verified against a real ROM image.
- Native C `float` support, bringing up SDCC's portable software float
  library (`_fsadd.c`, `_fsmul.c`, etc.) file by file — addition and
  32-bit multiply are verified correct end to end; multiplication of
  `float` values themselves is still being debugged.

See `sdcc/src/lh5801/gen.c` and recent commit history for the current
state.

## Companion project

[**pc1500emu**](https://github.com/pchambre/pc1500emu) is a from-scratch
PC-1500/LH5801 emulator built alongside this compiler port, used to
verify every ROM call and hardware interaction empirically against real
ROM dumps rather than by assumption.

## License

Unchanged from upstream SDCC: GPL, with the usual linking exception for
compiled output (see `sdcc/COPYING` and the license header in each
library source file).
