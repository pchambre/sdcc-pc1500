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

- **`sdcc/src/lh5801/`** — the LH5801 compiler backend itself. Values of
  one or two bytes live in the halves of the X/Y/U registers where SDCC's
  tree-decomposition register allocator (`ralloc2.cc`) finds that smaller,
  everything else at a fixed, directly-addressed memory location; A is
  only scratch. `--oldralloc` puts every value in memory, the original
  ("phase-1") design. Function locals that need memory are overlaid as a
  compiled stack per module (`overlay.c`): functions that are never active
  at the same time share it. A function that isn't `static` keeps memory of
  its own if it calls the library or another module (which could call it
  back), so declaring helpers `static` saves RAM; `--lh5801-no-overlay`
  turns overlaying off. The first 1-byte parameter is passed in A and the
  first 2-byte one in X (`main.c`'s `lh5801_reg_parm()`); the others go to
  the callee's `_PARM_n` bytes, and reentrant and varargs functions take
  theirs on the stack. 32-bit copies, constant stores, add, subtract,
  negate, shifts and ordered compares of values in memory call small
  runtime helpers (`device/lib/lh5801/_lh5801_*4.asm`, operand addresses
  inline after the call) instead of expanding byte by byte;
  `--opt-code-speed` keeps the inline code. See `gen.c`'s, `ralloc2.cc`'s
  and `overlay.c`'s own header comments for the design rationale.
- **`sdcc/sdas/aslh5801/`** — the LH5801 assembler, built on the shared
  `sdas` framework used by SDCC's other targets.
- **`sdcc/device/lib/lh5801/`** and **`sdcc/device/include/lh5801/`** — a
  small runtime library, and its header (`pc1500.h`), giving C programs
  access to real PC-1500 hardware and ROM functionality: LCD output
  (characters, strings, BASIC's CURSOR, display reverse), the keyboard
  (blocking and INKEY$-style), BASIC-INPUT-style line entry, the buzzer,
  the ON key, power-conscious idle/halt and delays, `exit()` back to
  BASIC, cassette I/O, and ROM-native numeric routines (the PC-1500's own
  floating-point format and math functions).
- **`sdcc/support/scripts/build-lh5801.sh`** — a wrapper script that
  compiles, assembles, and links C source files into a padding-free flat
  binary ready to load onto a real PC-1500 (or the emulator below) at a
  chosen address, pulling in only the runtime modules the program uses.
- **`sdcc/device/examples/lh5801/`** — example programs, including
  `blackjack/`, a port of Antony F. P. Vickery's 1984 BASIC Blackjack.

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
  target): for this port HOME holds only the entry label, and GSFINAL saves
  S for `exit()`, calls `_main` and returns to BASIC. Also,
  `printIvalFuncPtr()`/`printIvalPtr()` now set `addrtaken` on a function
  named in an initializer (`fp t[] = { f }`): SDCC only recorded an explicit
  `&f`, and this port's local overlay must know every function that can be
  called through a pointer. For other targets this only makes `addrtaken`
  more accurate.
- **`src/SDCCpeeph.c`** — `labelIsUncondJump()` picks each target's jump
  mnemonics; added the LH5801's (`jmp`/`bch`), and a FALSE return for a
  target with none listed instead of `strlen(NULL)`.
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
- **`device/include/stdbit.h`** — like most SDCC targets, the LH5801 CPU
  has no wide load/store instructions that force a byte order on multi-byte
  values (it moves one byte at a time); the storage convention is entirely
  the backend's own choice. This backend consistently chose big-endian
  (byte 0 = most significant byte). `__STDC_ENDIAN_NATIVE__` only
  special-cased `hc08`/`s08`/`stm8` — genuinely big-endian CPUs — as
  big-endian, defaulting every other port, LH5801 included, to little-endian
  regardless of what that port actually does. Left unset, portable library
  code that branches on this macro to pick a byte-order-dependent union
  layout (e.g. `device/lib/_mullong.c`'s multiply) silently computed wrong
  answers for this port. Declaring it here is this fork registering its own
  design choice, the same way every other change in this list registers
  something about the new target — not a defect in upstream SDCC.

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
sdcc/support/scripts/build-lh5801.sh myprogram.c [more.c ...] [load_address_hex]
```

The program runs by `CALL &<load address>` from BASIC, and returns to
BASIC when `main()` returns or `exit()` is called. The startup code
zeroes globals and copies initialized data on every CALL, so a program
can simply be run again. Its RAM includes uninitialized data after the
image, so reserve it from BASIC first with `NEW &<address>`. The script
prints that address, with the load address and image size. Load it at the
bottom of user RAM: `40C5` on a machine with RAM from 4000H, or `00C5`
with 16K at 0000H. See the script's own header comment for details.

### Building on Windows

The full `./configure && make` build above works under WSL/MSYS2 exactly
as on Linux and is the primary path for the compiler itself. But if you
just need **`sdaslh5801`** and **`sdld`** (the assembler and linker,
e.g. to drive [pc1500emu](https://github.com/pchambre/pc1500emu)'s
debugger or its VS Code build task) as native `.exe`s without a
WSL/MSYS2 dependency, upstream SDCC already ships hand-maintained MSVC
project files for every `sdas`/`sdld` backend (`sdas/<target>/*.vcxproj`)
— every one, that is, except `aslh5801`, presumably because it didn't
exist yet when those project files were last regenerated. This fork adds
that missing project file rather than introducing a whole second build
system:

1. **Generate `sdcc_vc.h`** (one-time; this is `sdccconf.h`'s equivalent
   for the MSVC build — see `sdcc/.gitignore`'s comment — and needs
   `gawk`, e.g. from a Git Bash/MSYS2 shell):
   ```sh
   cd sdcc
   gawk -f configure_vc.awk sdcc_vc.h.in > sdcc_vc.h
   ```
2. **Build the assembler and linker** with MSBuild (adjust the MSBuild
   path for your Visual Studio version/edition; `PlatformToolset` should
   match one actually installed — `v143` is VS2022's):
   ```sh
   MSBUILD="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
   "$MSBUILD" sdas/aslh5801/aslh5801.vcxproj -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v143
   "$MSBUILD" sdas/linksrc/aslink.vcxproj    -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v143
   ```
   Output lands at `sdcc/bin_vc/sdaslh5801.exe` and `sdcc/bin_vc/sdld.exe`.

Two things worth knowing if you're comparing this against the other
`sdas/*/*.vcxproj` files or trying to add a `.vcxproj` for another
target yourself:
- `aslh5801.vcxproj` and the fixed-up `aslink.vcxproj` deliberately don't
  reference `config.vcxproj` (the project that generates `sdcc_vc.h` and
  `sdas/linksrc/asxxxx_config.h` via a `gawk`-driven custom-build step)
  the way most of the other per-target projects do. Doing that build step
  through MSBuild needs `gawk` reachable from MSBuild's own process
  environment, not just an interactive shell, which isn't a given on a
  stock Windows install — and neither generated header is actually
  `#include`d by any source file these two targets compile (confirmed by
  grep), so the dependency was pure friction for no benefit. Generating
  `sdcc_vc.h` by hand once, as above, covers the one header that *is*
  real (it's directly included).
- `aslink.vcxproj` also had a stale `lks19.c` `ClCompile` entry (that file
  doesn't exist in this tree and isn't in `sdas/linksrc/Makefile.in`'s own
  `SRC` list either) — removed, not replaced, since the Makefile-driven
  build has never compiled it.

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
