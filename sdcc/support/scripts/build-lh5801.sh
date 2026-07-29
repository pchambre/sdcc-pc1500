#!/bin/bash
# build-lh5801.sh - compile a C source file for the PC-1500 (LH5801 SDCC
# port), link it against the ROM-calling runtime library
# (device/lib/lh5801/), and emit a padding-free flat binary ready to load
# onto a real PC-1500 (or pc1500emu) at a fixed address.
#
# Paul Chambre, 2026
#
# Usage: build-lh5801.sh <source.c> [base_addr_hex]
#
#   base_addr_hex   load address, in hex, no "0x" prefix (default: 4400).
#                   This must be real, writable RAM on a PC-1500 -- 4000-
#                   47FF is always present; 4800-6FFF needs the CE-158
#                   extension RAM module. See device/lib/lh5801/*.asm and
#                   docs/pc1500_hardware_reference.md (pc1500emu project)
#                   for the memory map this assumes.
#
# Output: <source_basename>-<base>-<call_addr>-pc1500.bin in the current
# directory, where <call_addr> is _main's real address -- that's the
# address to CALL from BASIC (not <base>, which is just where HOME/the
# startup trampoline lands; see the CALL-address discussion this tool's
# design came out of).
#
# Why three link passes: HOME/GSINIT/GSFINAL are fixed-size startup
# boilerplate, but CONST (string literals) and CODE both vary per
# program, and sdld places any *unbased* area by simple encounter-order
# address inheritance across the whole link (confirmed directly: an
# unbased CODE area silently inherits whatever address an earlier-
# encountered DATA area happened to leave the "current pointer" at,
# which is how the very first version of this workflow ended up with a
# multi-KB gap between CONST and CODE). Explicitly basing CODE right
# after CONST, and DATA right after CODE, is the only way to get a
# genuinely contiguous, gap-free image -- so pass 1 discovers
# HOME+GSINIT+GSFINAL+CONST's real total size, pass 2 (now that CODE is
# based correctly) discovers CODE's real size, and pass 3 produces the
# real output with DATA based right after that.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDCC_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

SDCC="$SDCC_ROOT/bin/sdcc"
ASLH5801="$SDCC_ROOT/bin/sdaslh5801"
SDLD="$SDCC_ROOT/bin/sdld"
MAKEBIN="$SDCC_ROOT/bin/makebin"
DEVICE_INCLUDE="$SDCC_ROOT/device/include"
DEVICE_LIB_LH5801="$SDCC_ROOT/device/lib/lh5801"

DEFAULT_BASE="4400"
EXT_RAM_START_HEX="4800"  # CE-158 extension RAM boundary -- see warning below
RESERVE_AREA_END_HEX="40C4"  # BASIC firmware's reserve area (manual 5-3-6):
                              # 4000H-40C4H is live interpreter state on a
                              # bare PC-1500, not free RAM, despite being
                              # physically writable -- confirmed on real
                              # hardware (see pc1500emu project notes)

usage() {
  echo "Usage: $(basename "$0") <source.c> [base_addr_hex]" >&2
  exit 1
}

[ $# -ge 1 ] || usage
SRC="$1"
BASE_HEX="${2:-$DEFAULT_BASE}"

[ -f "$SRC" ] || { echo "error: no such file: $SRC" >&2; exit 1; }
case "$SRC" in
  *.c) ;;
  *) echo "error: expected a .c source file, got: $SRC" >&2; exit 1 ;;
esac

for tool in "$SDCC" "$ASLH5801" "$SDLD" "$MAKEBIN"; do
  [ -x "$tool" ] || { echo "error: missing or not executable: $tool" >&2; exit 1; }
done

if [ $((16#$BASE_HEX)) -le $((16#$RESERVE_AREA_END_HEX)) ]; then
  echo "warning: base address 0x$BASE_HEX overlaps 0x0000-0x$RESERVE_AREA_END_HEX," \
       "the BASIC firmware's reserve area -- this is live interpreter" \
       "state on a bare PC-1500 (manual section 5-3-6), not free RAM," \
       "even though it's physically writable. Loading here will corrupt" \
       "BASIC. Use 0x$(printf '%04X' $(( 16#$RESERVE_AREA_END_HEX + 1 ))) or higher." >&2
fi

OUTDIR="$(pwd)"
SRC_ABS="$(cd "$(dirname "$SRC")" && pwd)/$(basename "$SRC")"
BASENAME="$(basename "$SRC" .c)"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
cd "$WORKDIR"

hex_to_dec() { echo $((16#$1)); }
BASE_DEC="$(hex_to_dec "$BASE_HEX")"

# --- compile + assemble the user's program -----------------------------
cp "$SRC_ABS" "$BASENAME.c"
"$SDCC" -S -mlh5801 -I"$DEVICE_INCLUDE" "$BASENAME.c" -o "$BASENAME.asm"
"$ASLH5801" -logs "$BASENAME.asm" >/dev/null

# --- assemble the ROM-calling runtime library ---------------------------
REL_ARGS=("$BASENAME.rel")
for asm in "$DEVICE_LIB_LH5801"/*.asm; do
  libbase="$(basename "$asm" .asm)"
  "$ASLH5801" -logs -o "$WORKDIR/$libbase.rel" "$asm" >/dev/null
  REL_ARGS+=("$libbase.rel")
done

# area_size <map_file> <area_name> -> hex size (0 if the area doesn't
# appear in the map at all, e.g. a program with no string literals has
# no real CONST content).
area_size() {
  local map="$1" area="$2" size
  size="$(awk -v a="$area" '$1==a{print $3}' "$map" | head -1)"
  [ -n "$size" ] || size="00000000"
  echo "$size"
}

write_lnk() {
  # write_lnk <name> <home_base_hex> [code_base_hex] [data_base_hex]
  local name="$1" home="$2" code="${3:-}" data="${4:-}"
  {
    echo "-muwx"
    echo "-i $name"
    echo "-b HOME = 0x$home"
    [ -n "$code" ] && echo "-b CODE = 0x$code"
    [ -n "$data" ] && echo "-b DATA = 0x$data"
    for f in "${REL_ARGS[@]}"; do echo "$f"; done
    echo
    echo "-e"
  } > "$name.lnk"
}

# --- pass 1: discover HOME+GSINIT+GSFINAL+CONST's real total size ------
write_lnk pass1 "$BASE_HEX"
"$SDLD" -nf pass1 >/dev/null

home_sz="$(area_size pass1.map HOME)"
gsinit_sz="$(area_size pass1.map GSINIT)"
gsfinal_sz="$(area_size pass1.map GSFINAL)"
const_sz="$(area_size pass1.map CONST)"
prelude_sz=$(( 16#$home_sz + 16#$gsinit_sz + 16#$gsfinal_sz + 16#$const_sz ))
code_base_dec=$(( BASE_DEC + prelude_sz ))
code_base_hex="$(printf '%04X' "$code_base_dec")"

# --- pass 2: CODE now based correctly -- discover its real total size --
write_lnk pass2 "$BASE_HEX" "$code_base_hex"
"$SDLD" -nf pass2 >/dev/null

code_sz="$(area_size pass2.map CODE)"
data_base_dec=$(( code_base_dec + 16#$code_sz ))
data_base_hex="$(printf '%04X' "$data_base_dec")"

# --- pass 3: final, fully contiguous link ------------------------------
write_lnk pass3 "$BASE_HEX" "$code_base_hex" "$data_base_hex"
"$SDLD" -nf pass3 >/dev/null

main_addr="$(awk '$2=="_main"{print $1; exit}' pass3.map)"
[ -n "$main_addr" ] || { echo "error: _main not found in link map -- does $SRC define main()?" >&2; exit 1; }
main_hex="$(echo "$main_addr" | sed 's/^0*//')"
[ -n "$main_hex" ] || main_hex="0"

"$MAKEBIN" -p -o "$BASE_DEC" pass3.ihx pass3.bin >/dev/null

data_sz="$(area_size pass3.map DATA)"
data_end_dec=$(( data_base_dec + 16#$data_sz ))
if [ "$data_end_dec" -gt "$(hex_to_dec "$EXT_RAM_START_HEX")" ]; then
  echo "warning: static data extends to 0x$(printf '%04X' "$data_end_dec")," \
       "past 0x$EXT_RAM_START_HEX -- this program needs the CE-158" \
       "extension RAM module on real hardware (or emulator" \
       "Bus::setExtRam4800Size())." >&2
fi

OUT="$OUTDIR/${BASENAME}-${BASE_HEX}-${main_hex}-pc1500.bin"
cp pass3.bin "$OUT"

BIN_SIZE="$(stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT")"
echo "$OUT"
echo "  size: $BIN_SIZE bytes, load at 0x$BASE_HEX, call at 0x$main_hex"
