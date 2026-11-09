#!/bin/bash
# build-lh5801.sh - compile C source files for the PC-1500 (LH5801 SDCC
# port), link them against the PC-1500 runtime library, and emit a
# padding-free flat binary ready to load onto a real PC-1500 (or
# pc1500emu) at a fixed address.
#
# Paul Chambre, 2026
#
# Usage: build-lh5801.sh [--banks=DEVICE] [--pc1500a] <source.c|object.rel|library.lib> [more ...] [base_addr_hex]
#
#   --banks=DEVICE  bank-switched code (2026-11-09): files with `#pragma bank N`
#                   go in bank N of DEVICE's 0000H-3FFFH window, everything
#                   else at the base address, which must then be 4000H or above.
#                   DEVICE is card (the internal expansion card, BANK 0-4),
#                   ce163 (banks 0-1), ce1638 (0-7) or ce163f (RAM 0-7,
#                   flash 9-15). See device/include/lh5801/pc1500_bank.h.
#                   Each bank's image starts at the bank's BASIC program
#                   start with FFH -- an empty BASIC program, so BASIC (and
#                   the card's BANK, the vendors' switchers) leave the
#                   bank alone -- and the code follows. Writes one image
#                   per bank besides the fixed one, and LOAD.TXT.
#   --pc1500a       a CE device's bank switch at 6800H, not 5800H
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
# current directory, and the same image as an SD-card file for the
# expansion module's SDLOAD M: <BASENAME>.BIN (the name upper-cased and cut
# to 8.3), the image after a 4-byte header -- load address, then the
# address SDLOAD M CALLs (the same), both big-endian. NEW first, then
# SDLOAD M "<BASENAME>.BIN" loads and starts it. CALL it from BASIC at <base> itself: HOME (the
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
# compile and assemble with this backend. Not the ones the port has its
# own of: device/lib/lh5801/string_*.asm, on TIN/SIN/LIN/CIN,
# printf_large.c (the same formatting in less code) and the float
# arithmetic, comparisons, conversions and sqrtf in assembly
# (_lh5801_fs.asm and the modules it lists).
RUNTIME_EXCLUDE=" memset strcpy printf_large _fsadd _fssub _fsmul _fsdiv _fslt _fseq _fsneq 
  _fs2ulong _fs2slong _fs2uint _fs2sint _fs2uchar _fs2schar 
  _ulong2fs _slong2fs _uint2fs _sint2fs _uchar2fs _schar2fs sqrtf "
# plus the ones device/lib/Makefile has each port choose (its SOURCES_*
# lists): integer and long long arithmetic, conversions, heap, stdio
RUNTIME_SOURCES=(
  _muluchar _mulschar _mulint _mullong _mullonglong
  _divuchar _divschar _moduchar _modschar
  _divuint _divsint _moduint _modsint _divulong
  _divslonglong _divulonglong _modslonglong _modulonglong
  _srulonglong _srslonglong _slulonglong _slslonglong
  __itoa __ltoa strnlen memcpy
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
BANKS=""
PC1500A=0
for arg in "$@"; do
  case "$arg" in
    --banks=*) BANKS="${arg#--banks=}" ;;
    --pc1500a) PC1500A=1 ;;
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

# --banks: each device's banks, where a bank's BASIC program starts (the
# image's first byte, an FFH), and how its switch module is assembled
# (device/lib/lh5801/bank/).
case "$BANKS" in
  "") ;;
  card)   BANK_MIN=0; BANK_MAX=4;  BANK_PROG=00C5 ;;
  ce163)  BANK_MIN=0; BANK_MAX=1;  BANK_PROG=00C5; BANK_HOME_AT=0 ;;
  ce1638) BANK_MIN=0; BANK_MAX=7;  BANK_PROG=0104; BANK_HOME_AT=0x00D4 ;;
  ce163f) BANK_MIN=0; BANK_MAX=15; BANK_PROG=0112; BANK_HOME_AT=0x00E2 ;;
  *) echo "error: --banks= takes card, ce163, ce1638 or ce163f, got: $BANKS" >&2; exit 1 ;;
esac
if [ -n "$BANKS" ] && [ $((16#$BASE_HEX)) -lt $((16#4000)) ]; then
  echo "error: with --banks, the base address must be 4000H or above (the banks are 0000H-3FFFH): $BASE_HEX" >&2
  exit 1
fi
BANK_PORT=$([ "$PC1500A" = 1 ] && echo 0x68 || echo 0x58)

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

# --banks: the device's switch, after main's module (its GSINIT code runs
# after startup has zeroed DATA)
if [ -n "$BANKS" ]; then
  case "$BANKS" in
    card) cp "$DEVICE_LIB_LH5801/bank/bank_card.asm" "$WORKDIR/bank_dev.asm" ;;
    *) { echo "BANK_PORT = $BANK_PORT"; echo "BANK_HOME_AT = $BANK_HOME_AT";
         cat "$DEVICE_LIB_LH5801/bank/bank_ce.asm"; } > "$WORKDIR/bank_dev.asm" ;;
  esac
  "$ASLH5801" -logs "$WORKDIR/bank_dev.asm" >/dev/null
  REL_ARGS=("${REL_ARGS[0]}" bank_dev.rel "${REL_ARGS[@]:1}")
fi

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

write_lnk() {  # write_lnk <name> <area=hex_base>... [-B area]...
  local name="$1"; shift
  {
    echo "-muwx"
    echo "-i $name"
    for b in "$@"; do
      case "$b" in
        -B*) echo "$b" ;;    # sdld: write only these areas' data
        *) echo "-a ${b%%=*} = 0x${b#*=}" ;;
      esac
    done
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
# Any other area -- a named address space's (__addressmod), say -- goes in
# RAM after INITIALIZED (2026-11-03): left unbased, sdld put bug-2995's
# space_a/space_b at CODE's own address, and storing to them overwrote
# code. Not SSEG (above), nor DSEG: 128 bytes with no symbol in them in
# every program, kept where they were (at 0, unused).
# --banks: CODE_<n>, bank n's code, at the bank's BASIC program start + 1
# in every bank -- the same addresses, so sdld writes each on its own
# (-B, below).
BANK_AREAS=()
while read -r area; do
  case " ${LAYOUT[*]} SSEG DSEG " in *" $area "*) continue ;; esac
  if [[ "$area" =~ ^CODE_([0-9]+)$ ]]; then
    n="${BASH_REMATCH[1]}"
    [ -n "$BANKS" ] ||
      { echo "error: $area: \`#pragma bank $n\` needs --banks=" >&2; exit 1; }
    { [ "$n" -ge "$BANK_MIN" ] && [ "$n" -le "$BANK_MAX" ]; } ||
      { echo "error: $area: $BANKS has banks $BANK_MIN-$BANK_MAX" >&2; exit 1; }
    [ "$BANKS$n" != ce163f8 ] ||
      { echo "error: CODE_8: the CE-163F's bank 8 holds its vendor's utilities" >&2; exit 1; }
    BANK_AREAS+=("$area")
    continue
  fi
  LAYOUT+=("$area")
done < <(awk '$NF ~ /^\(REL/ && $4 == "=" {print $1}' pass1.map)
BANK_CODE_HEX=""
if [ -n "$BANKS" ]; then
  BANK_CODE_HEX="$(printf '%04X' $(( 16#$BANK_PROG + 1 )))"
  for area in "${BANK_AREAS[@]+"${BANK_AREAS[@]}"}"; do
    size=$(area_size pass1.map "$area")
    [ $(( 16#$BANK_CODE_HEX + size )) -le $((16#4000)) ] ||
      { echo "error: $area is $size bytes, past 3FFFH from $BANK_CODE_HEX" >&2; exit 1; }
  done
fi
BASES=()
addr=$BASE_DEC
image_end=$addr
for area in "${LAYOUT[@]}"; do
  BASES+=("$area=$(printf '%04X' "$addr")")
  addr=$(( addr + $(area_size pass1.map "$area") ))
  [ "$area" = CODE ] && image_end=$addr
done
ram_end=$addr
for area in "${BANK_AREAS[@]+"${BANK_AREAS[@]}"}"; do BASES+=("$area=$BANK_CODE_HEX"); done
if [ ${#BANK_AREAS[@]} -gt 0 ]; then
  # the fixed image without the banks, which share addresses
  FIXED_ONLY=()
  for area in "${LAYOUT[@]}"; do FIXED_ONLY+=("-B $area"); done
  write_lnk pass2 "${BASES[@]}" "${FIXED_ONLY[@]}"
else
  write_lnk pass2 "${BASES[@]}"
fi
"$SDLD" -nf pass2 >/dev/null

for area in "${LAYOUT[@]}" "${BANK_AREAS[@]+"${BANK_AREAS[@]}"}"; do
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
SD_NAME="$(echo "${BASENAME:0:8}" | tr 'a-z' 'A-Z').BIN"
SD_HDR="\\$(printf '%03o' $(( BASE_DEC >> 8 )))\\$(printf '%03o' $(( BASE_DEC & 255 )))"
{ printf "$SD_HDR$SD_HDR"; cat "$OUT.bin"; } > "$OUTDIR/$SD_NAME"

# --banks: each bank's image, FFH (an empty BASIC program) then its code;
# the SD file's header has CALL address 0 (none).
BANK_FILES=()
for area in "${BANK_AREAS[@]+"${BANK_AREAS[@]}"}"; do
  n="${area#CODE_}"
  write_lnk "bank$n" "${BASES[@]}" "-B $area"
  "$SDLD" -nf "bank$n" >/dev/null
  "$MAKEBIN" -p -s 65536 -o "$(( 16#$BANK_CODE_HEX ))" "bank$n.ihx" "bank$n.code" >/dev/null
  { printf '\377'; cat "bank$n.code"; } > "$OUT-b$n.bin"
  prog_dec=$(( 16#$BANK_PROG ))
  hdr="\\$(printf '%03o' $(( prog_dec >> 8 )))\\$(printf '%03o' $(( prog_dec & 255 )))"
  bank_sd="$(echo "${BASENAME:0:5}B$n" | tr 'a-z' 'A-Z').BIN"
  { printf "$hdr\000\000"; cat "$OUT-b$n.bin"; } > "$OUTDIR/$bank_sd"
  BANK_FILES+=("$n:$bank_sd:$(stat -c%s "$OUT-b$n.bin")")
done

# --banks=card: the SD file holds the banks too, for SDLOAD M to load in
# one go (RP2350/keywords.c, segload): FFH 'B', the CALL address, a count,
# then per segment bank (FFH: the fixed code), address, length (all
# big-endian), then the segments' bytes in that order -- the banks first.
if [ "$BANKS" = card ] && [ ${#BANK_FILES[@]} -gt 0 ]; then
  oct() { printf '\\%03o' "$1"; }
  be16() { oct $(( $1 >> 8 )); oct $(( $1 & 255 )); }
  hdr="$(oct 255)$(oct 66)$(be16 "$BASE_DEC")$(oct $(( ${#BANK_FILES[@]} + 1 )))"
  for f in "${BANK_FILES[@]}"; do
    IFS=: read -r n sd size <<< "$f"
    hdr="$hdr$(oct "$n")$(be16 $(( 16#$BANK_PROG )))$(be16 "$size")"
  done
  hdr="$hdr$(oct 255)$(be16 "$BASE_DEC")$(be16 "$(stat -c%s "$OUT.bin")")"
  {
    printf "$hdr"
    for f in "${BANK_FILES[@]}"; do cat "$OUT-b${f%%:*}.bin"; done
    cat "$OUT.bin"
  } > "$OUTDIR/$SD_NAME"
fi

BIN_SIZE="$(stat -c%s "$OUT.bin" 2>/dev/null || stat -f%z "$OUT.bin")"
echo "$OUT.bin"
echo "  image: $BIN_SIZE bytes at 0x$BASE_HEX-0x$(printf '%04X' $(( image_end - 1 )))," \
     "RAM to 0x$(printf '%04X' $(( ram_end - 1 ))); CALL &$BASE_HEX"
# BASIC's NEW <address> moves BASIC's program area up, reserving everything
# below it -- the program's RAM-only areas included, which the startup
# code zeroes on every CALL.
echo "  reserve it first with NEW &$(printf '%04X' "$ram_end") (before loading it)"
if [ "$BANKS" = card ] && [ ${#BANK_FILES[@]} -gt 0 ]; then
  echo "  SD card: $SD_NAME -- SDLOAD M \"$SD_NAME\" loads it and its banks and starts it"
else
  echo "  SD card: $SD_NAME -- SDLOAD M \"$SD_NAME\" loads and starts it"
fi

# --banks: LOAD.TXT, the steps to load it all, typed at the prompt (in
# RUN mode, nothing running: a running program can't switch the bank
# under itself).
if [ ${#BANK_FILES[@]} -gt 0 ]; then
  port=$([ "$PC1500A" = 1 ] && echo 68 || echo 58)
  {
    echo "$BASENAME: fixed code at &$BASE_HEX, banked code for $BANKS."
    echo "Type each line at the prompt: NEW in PRO mode, the rest in RUN"
    echo "mode. A bank holding code can't hold a BASIC program: the one"
    echo "there is lost."
    echo
    case "$BANKS" in
      card)
        echo "$SD_NAME holds the fixed code and every bank:"
        echo "  NEW &$(printf '%04X' "$ram_end")"
        echo "  SDLOAD M \"$SD_NAME\"        (loads it all and starts it; CALL &$BASE_HEX after)"
        echo
        echo "To load one bank's code on its own (BANK needs BASIC's program"
        echo "still in 0000H-3FFFH, so before that NEW):"
        for f in "${BANK_FILES[@]}"; do
          IFS=: read -r n sd size <<< "$f"
          echo "  BANK $n"
          echo "  SDLOAD M \"$sd\"        ($size bytes at &$BANK_PROG)"
        done
        echo "  BANK 0                      (the bank BASIC uses)"
        ;;
      *)
        echo "The banks first, while BASIC's program is still in 0000H-3FFFH:"
        home="H=PEEK &D4"; [ "$BANKS" = ce163f ] && home="H=PEEK &E2"
        [ "$BANKS" = ce163 ] && home="H=0                  (start from bank 0)"
        echo "  $home"
        for f in "${BANK_FILES[@]}"; do
          IFS=: read -r n sd size <<< "$f"
          echo "  POKE &$port$(printf '%02X' "$n"),0"
          echo "  CLOAD M \"${BASENAME:0:12}B$n\";&$BANK_PROG     ($BASENAME-$BASE_HEX-pc1500-b$n.bin, $size bytes)"
        done
        echo "  POKE &${port}00+H,0          (back to the BASIC bank)"
        echo
        echo "Then the fixed code:"
        echo "  NEW &$(printf '%04X' "$ram_end")"
        echo "  CLOAD M \"${BASENAME:0:16}\";&$BASE_HEX      ($BASENAME-$BASE_HEX-pc1500.bin)"
        echo "  CALL &$BASE_HEX"
        ;;
    esac
    if [ "$BANKS" = ce163f ]; then
      echo
      echo "Banks 9-15 are flash: load the image into a free RAM bank as"
      echo "above, then switch to that bank and copy it with the vendor's"
      echo "utilities, X= and CALL on separate lines:"
      echo "  X=<that RAM bank>"
      echo "  CALL &E3,X"
      echo "  X=<the flash bank>"
      echo "  CALL &C6,X"
    fi
  } > "$OUTDIR/LOAD.TXT"
  for f in "${BANK_FILES[@]}"; do
    IFS=: read -r n sd size <<< "$f"
    echo "  bank $n: $BASENAME-$BASE_HEX-pc1500-b$n.bin, $size bytes at 0x$BANK_PROG (SD: $sd)"
  done
  echo "  LOAD.TXT: how to load them ($BANKS)"
fi
