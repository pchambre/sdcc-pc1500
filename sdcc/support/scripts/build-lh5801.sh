#!/bin/bash
# build-lh5801.sh - compile C source files for the PC-1500 (LH5801 SDCC
# port), link them against the PC-1500 runtime library, and emit a
# padding-free flat binary ready to load onto a real PC-1500 (or
# pc1500emu) at a fixed address.
#
# Paul Chambre, 2026
#
# Usage: build-lh5801.sh <source.c|object.rel|library.lib> [more ...] [base_addr_hex]
#
#   .rel            an object already compiled with sdcc -c -mlh5801 (SDCC's
#                   regression suite links its tests this way)
#   .lib            an sdld library of such objects, searched like the
#                   runtime library: only the modules referenced are linked
#
#   base_addr_hex   load address, in hex, no "0x" prefix (default: 4400).
#                   This must be real, writable RAM on a PC-1500 that BASIC
#                   isn't using -- 4000-47FF is always present; 4800-6FFF
#                   needs the CE-158 extension RAM module. See
#                   device/lib/lh5801/*.asm and docs/pc1500_hardware_reference.md
#                   (pc1500emu project) for the memory map this assumes.
#
# Output: <first_source_basename>-<base>-pc1500.bin (and its .map) in the
# current directory. CALL it from BASIC at <base> itself: HOME (the
# image's first area) holds the startup code's entry label, which falls
# through into GSINIT -- zero DATA, copy INITIALIZER into INITIALIZED, run
# every module's static initializers -- and then GSFINAL calls main() and
# returns to BASIC (see src/lh5801/main.c's lh5801_genInitStartup()).
#
# The runtime is a library, not a fixed list of objects: every
# device/lib/lh5801/*.asm and *.c file, plus SDCC's generic C library
# (device/lib/incl.mk's COMMON_* lists: stdio, stdlib, string, ctype,
# math, float, long long, stdc_bit...), is compiled once into a cached directory and listed in
# a plain-text sdld library (one .rel name per line, see
# sdas/linksrc/lklib.c), so the linker only pulls in the modules a
# program actually references. The cache is rebuilt per file whenever a
# source, pc1500.h, the compiler or the assembler is newer than its .rel.
#
# Layout: two link passes. Pass 1 bases only HOME, to learn every area's
# real size (sizes don't depend on addresses -- every instruction this
# port emits has a fixed size). Pass 2 bases every area explicitly, back
# to back: HOME, GSINIT, GSFINAL, CONST, INITIALIZER, CODE (the image),
# then the RAM-only areas DATA and INITIALIZED. sdld places any
# *unbased* area by encounter-order address inheritance across the whole
# link (confirmed directly: an unbased CODE area silently inherited
# whatever address an earlier-encountered DATA area happened to leave the
# "current pointer" at), so nothing is left unbased.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDCC_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

SDCC="$SDCC_ROOT/bin/sdcc"
ASLH5801="$SDCC_ROOT/bin/sdaslh5801"
SDLD="$SDCC_ROOT/bin/sdld"
MAKEBIN="$SDCC_ROOT/bin/makebin"
DEVICE_INCLUDE="$SDCC_ROOT/device/include"
# pc1500.h itself lives in the port-specific subdirectory, not directly
# under device/include/ -- sdcc doesn't add this automatically for -mlh5801,
# confirmed live: compiling with only -I"$DEVICE_INCLUDE" fails with
# "pc1500.h: No such file or directory" on the example program's own
# #include <pc1500.h>.
DEVICE_INCLUDE_LH5801="$SDCC_ROOT/device/include/lh5801"
DEVICE_LIB_LH5801="$SDCC_ROOT/device/lib/lh5801"
DEVICE_LIB="$SDCC_ROOT/device/lib"
# SDCCFLAGS (environment): extra sdcc options for every compile, the
# runtime library's included -- e.g. SDCCFLAGS=--oldralloc. Each set of
# flags gets its own library cache.
SDCCFLAGS="${SDCCFLAGS:-}"
LIB_CACHE="$SDCC_ROOT/device/lib/build/lh5801"
if [ -n "$SDCCFLAGS" ]; then
  LIB_CACHE="$LIB_CACHE-$(echo "$SDCCFLAGS" | md5sum | cut -c1-8)"
fi
LIB_NAME="pc1500"
# SDCC's generic C library: every source in device/lib/incl.mk's COMMON_*
# lists (what every port's library is built from), compiled the way
# device/lib/Makefile does -- sdcc doesn't pull these in on its own. All
# compile and assemble with this backend. Not the ones the port has in
# assembler: device/lib/lh5801/string_*.asm, on TIN/SIN/LIN/CIN.
RUNTIME_EXCLUDE=" _memset "
# plus the ones device/lib/Makefile has each port choose (its SOURCES_*
# lists): integer and long long arithmetic, conversions, heap, stdio
RUNTIME_SOURCES=(
  _muluchar _mulschar _mulint _mullong _mullonglong
  _divuchar _divschar _moduchar _modschar
  _divuint _divsint _moduint _modsint _divulong
  _divslonglong _divulonglong _modslonglong _modulonglong
  _srulonglong _srslonglong _slulonglong _slslonglong
  __itoa __ltoa _strcmp strnlen memcpy _memmove
  div ldiv lldiv imaxdiv vprintf sprintf
)
for f in $(sed -n '/^COMMON_[A-Z]* *=/,/^$/p' "$SDCC_ROOT/device/lib/incl.mk"); do
  [[ "$f" == *.c ]] || continue          # the names, not "=" or "\"
  f="${f%.c}"
  [[ "$RUNTIME_EXCLUDE" == *" $f "* ]] || RUNTIME_SOURCES+=("$f")
done
LIBFLAGS="--std-c23"

DEFAULT_BASE="4400"
EXT_RAM_START_HEX="4800"  # CE-158 extension RAM boundary -- see warning below
RESERVE_AREA_END_HEX="40C4"  # BASIC firmware's reserve area (manual 5-3-6):
                              # 4000H-40C4H is live interpreter state on a
                              # bare PC-1500, not free RAM, despite being
                              # physically writable -- confirmed on real
                              # hardware (see pc1500emu project notes)

usage() {
  echo "Usage: $(basename "$0") <source.c|object.rel|library.lib> [more ...] [base_addr_hex]" >&2
  exit 1
}

[ $# -ge 1 ] || usage
SRCS=()
LIBS=()
BASE_HEX="$DEFAULT_BASE"
for arg in "$@"; do
  case "$arg" in
    *.c|*.rel)
      [ -f "$arg" ] || { echo "error: no such file: $arg" >&2; exit 1; }
      SRCS+=("$(cd "$(dirname "$arg")" && pwd)/$(basename "$arg")")
      ;;
    *.lib)
      [ -f "$arg" ] || { echo "error: no such file: $arg" >&2; exit 1; }
      LIBS+=("$(cd "$(dirname "$arg")" && pwd)/$(basename "$arg")")
      ;;
    *)
      [[ "$arg" =~ ^[0-9A-Fa-f]{1,4}$ ]] ||
        { echo "error: expected a .c, .rel or .lib file or a hex base address, got: $arg" >&2; exit 1; }
      BASE_HEX="$arg"
      ;;
  esac
done
[ ${#SRCS[@]} -ge 1 ] || usage

for tool in "$SDCC" "$ASLH5801" "$SDLD" "$MAKEBIN"; do
  [ -x "$tool" ] || { echo "error: missing or not executable: $tool" >&2; exit 1; }
done

if [ $((16#$BASE_HEX)) -ge $((16#4000)) ] && [ $((16#$BASE_HEX)) -le $((16#$RESERVE_AREA_END_HEX)) ]; then
  echo "warning: base address 0x$BASE_HEX overlaps 0x4000-0x$RESERVE_AREA_END_HEX," \
       "the BASIC firmware's reserve area on a bare PC-1500 -- this is live" \
       "interpreter state (manual section 5-3-6), not free RAM, even though" \
       "it's physically writable. Loading here will corrupt BASIC." >&2
fi

OUTDIR="$(pwd)"
BASENAME="$(basename "${SRCS[0]}")"; BASENAME="${BASENAME%.*}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

compile_c() {  # compile_c <src.c> <out_dir> -> <out_dir>/<name>.rel
  local src="$1" out="$2" flags="${3:-}" name
  name="$(basename "$src" .c)"
  # shellcheck disable=SC2086 -- SDCCFLAGS and flags are word lists
  "$SDCC" -S -mlh5801 $SDCCFLAGS $flags -I"$DEVICE_INCLUDE" -I"$DEVICE_INCLUDE_LH5801" "$src" -o "$out/$name.asm"
  "$ASLH5801" -logs "$out/$name.asm" >/dev/null
}

# --- the runtime library (cached) --------------------------------------
mkdir -p "$LIB_CACHE"
LIB_SRCS=("$DEVICE_LIB_LH5801"/*.asm)
for f in "$DEVICE_LIB_LH5801"/*.c; do [ -e "$f" ] && LIB_SRCS+=("$f"); done
for f in "${RUNTIME_SOURCES[@]}"; do LIB_SRCS+=("$DEVICE_LIB/$f.c"); done
# Parallel builds (make -j in SDCC's regression suite) share the cache:
# modules are rebuilt in a private directory and renamed into place, and
# each build has its own list -- in WORKDIR, its entries reaching the cache
# through a symlink, since sdld resolves them relative to the list's
# directory (and a rename over a shared file isn't atomic on /mnt/c).
LIB_TMP="$(mktemp -d "$LIB_CACHE/tmp.XXXXXX")"
trap 'rm -rf "$WORKDIR" "$LIB_TMP"' EXIT
ln -s "$LIB_CACHE" "$WORKDIR/lib"
LIB_LIST="$WORKDIR/$LIB_NAME.lib"
for src in "${LIB_SRCS[@]}"; do
  name="${src##*/}"; name="${name%.*}"
  rel="$LIB_CACHE/$name.rel"
  if [ ! -f "$rel" ] || [ "$src" -nt "$rel" ] || [ "$SDCC_ROOT/src/sdcc" -nt "$rel" ] ||
     [ "$ASLH5801" -nt "$rel" ] || [ "$DEVICE_INCLUDE_LH5801/pc1500.h" -nt "$rel" ]; then
    case "$src" in
      *.asm) "$ASLH5801" -logs -o "$LIB_TMP/$name.rel" "$src" >/dev/null ;;
      "$DEVICE_LIB"/*.c) compile_c "$src" "$LIB_TMP" "$LIBFLAGS" ;;
      *.c) compile_c "$src" "$LIB_TMP" ;;
    esac
    for f in "$LIB_TMP/$name".*; do
      [ "$f" = "$LIB_TMP/$name.rel" ] || mv -f "$f" "$LIB_CACHE/"
    done
    mv -f "$LIB_TMP/$name.rel" "$rel"
  fi
  echo "lib/$name" >> "$LIB_LIST"
done

# --- the program's own sources ----------------------------------------
# The module defining main() goes first: its GSINIT holds the startup
# code, which has to run before any other module's static initializers.
REL_ARGS=()
for src in "${SRCS[@]}"; do
  case "$src" in
    *.rel)
      name="$(basename "$src" .rel)"
      cp "$src" "$WORKDIR/$name.rel"
      # sdld -u updates the listing next to each object
      [ -f "${src%.rel}.lst" ] && cp "${src%.rel}.lst" "$WORKDIR/$name.lst"
      ;;
    *) compile_c "$src" "$WORKDIR"; name="$(basename "$src" .c)" ;;
  esac
  if grep -q '^S _main Def' "$WORKDIR/$name.rel"; then
    REL_ARGS=("$name.rel" "${REL_ARGS[@]+"${REL_ARGS[@]}"}")
  else
    REL_ARGS+=("$name.rel")
  fi
done
grep -q '^S _main Def' "$WORKDIR/${REL_ARGS[0]}" ||
  { echo "error: no source file defines main()" >&2; exit 1; }

cd "$WORKDIR"

hex_to_dec() { echo $((16#$1)); }
BASE_DEC="$(hex_to_dec "$BASE_HEX")"

# area_size <map_file> <area_name> -> size in decimal (0 if the area
# doesn't appear in the map at all, e.g. a program with no string
# literals has no CONST content).
area_size() {
  local size
  # The size column is 8 hex digits -- unlike the "User Base Address
  # Definitions" section's own "HOME = 0x4400" lines, which also start
  # with the area name.
  size="$(awk -v a="$2" '$1==a && $3 ~ /^[0-9A-Fa-f]+$/ {print $3}' "$1" | head -1)"
  echo $((16#${size:-0}))
}

write_lnk() {  # write_lnk <name> <area=hex_base>...
  local name="$1"; shift
  {
    echo "-muwx"
    echo "-i $name"
    for b in "$@"; do echo "-b ${b%%=*} = 0x${b#*=}"; done
    for l in "${LIBS[@]+"${LIBS[@]}"}"; do
      echo "-k $(dirname "$l")"
      echo "-l $(basename "$l" .lib)"
    done
    echo "-k $WORKDIR"
    echo "-l $LIB_NAME"
    for f in "${REL_ARGS[@]}"; do echo "$f"; done
    echo
    echo "-e"
  } > "$name.lnk"
}

# --- pass 1: every area's real size ------------------------------------
write_lnk pass1 "HOME=$BASE_HEX"
"$SDLD" -nf pass1 >/dev/null

# --- pass 2: every area based explicitly, back to back -----------------
# Not SSEG: SDCCglue.c's one-byte __start__stack placeholder is never
# used (a program runs on BASIC's own stack), emits no bytes, and sdld
# keeps it at 0 whatever base it's given.
LAYOUT=(HOME GSINIT GSFINAL CONST INITIALIZER CODE DATA INITIALIZED)
BASES=()
addr=$BASE_DEC
image_end=$addr
for area in "${LAYOUT[@]}"; do
  BASES+=("$area=$(printf '%04X' "$addr")")
  addr=$(( addr + $(area_size pass1.map "$area") ))
  [ "$area" = CODE ] && image_end=$addr
done
ram_end=$addr
write_lnk pass2 "${BASES[@]}"
"$SDLD" -nf pass2 >/dev/null

for area in "${LAYOUT[@]}"; do
  [ "$(area_size pass1.map "$area")" = "$(area_size pass2.map "$area")" ] ||
    { echo "error: area $area changed size between link passes" >&2; exit 1; }
done

# -s 65536: makebin's buffer defaults to 32K, too small for an image
# that ends above 7FFFH.
"$MAKEBIN" -p -s 65536 -o "$BASE_DEC" pass2.ihx pass2.bin >/dev/null

if [ "$BASE_DEC" -lt "$(hex_to_dec "$EXT_RAM_START_HEX")" ] &&
   [ "$ram_end" -gt "$(hex_to_dec "$EXT_RAM_START_HEX")" ]; then
  echo "warning: the program's RAM extends to 0x$(printf '%04X' "$ram_end")," \
       "past 0x$EXT_RAM_START_HEX -- this program needs the CE-158" \
       "extension RAM module on real hardware (or emulator" \
       "Bus::setExtRam4800Size())." >&2
fi

OUT="$OUTDIR/${BASENAME}-${BASE_HEX}-pc1500"
cp pass2.bin "$OUT.bin"
cp pass2.map "$OUT.map"

BIN_SIZE="$(stat -c%s "$OUT.bin" 2>/dev/null || stat -f%z "$OUT.bin")"
echo "$OUT.bin"
echo "  image: $BIN_SIZE bytes at 0x$BASE_HEX-0x$(printf '%04X' $(( image_end - 1 )))," \
     "RAM to 0x$(printf '%04X' $(( ram_end - 1 ))); CALL &$BASE_HEX"
# BASIC's NEW <address> moves BASIC's program area up, reserving everything
# below it -- the program's RAM-only areas included, which the startup
# code zeroes on every CALL.
echo "  reserve it first with NEW &$(printf '%04X' "$ram_end") (before loading it)"
