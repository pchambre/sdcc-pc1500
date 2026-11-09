#!/bin/bash
# banked.sh - the LH5801 port's banked-code test (2026-11-09): builds
# banked/ (main in fixed code, banks 1 and 2) with build-lh5801.sh
# --banks= for each device and runs it on an emulated PC-1500 with that
# device, the banks' images loaded as LOAD.TXT says. Checks the program's
# own result (OK, see banked/banked.c), then CALLs it again with doExit
# set -- exit() from inside bank 1 -- and that the starting bank (0) is
# back in afterwards.
#
# Paul Chambre, 2026
#
# Usage: banked.sh [device ...]     (default: ce1638 ce163f card)
#
# Needs PC1500RUNNER and PC1500ROM as run.sh does, and for the card
# PC1500EXPROM: the expansion module's ROM (rom_8800.bin, from the
# PC1500-PSOC5 repo's Design01_NonDMA_8K_PV_Swap.cydsn/rom) -- its
# BANK_RAW_ENTRY at 8819H is the card's switch. Without it the card is
# skipped. Exit status: the number of failed devices.

set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDCC_ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$SDCC_ROOT/support/scripts/build-lh5801.sh"
SRC="$HERE/banked"
LOAD=40C5

: "${PC1500RUNNER:?set PC1500RUNNER to pc1500runner (pc1500emu tools/runner)}"
: "${PC1500ROM:?set PC1500ROM to ROM1.BIN}"

hostpath() {
  case "$PC1500RUNNER" in
    *.exe) wslpath -w "$1" ;;
    *) echo "$1" ;;
  esac
}

DEVICES=("$@")
[ ${#DEVICES[@]} -gt 0 ] || DEVICES=(ce1638 ce163f card)

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

failed=0
for dev in "${DEVICES[@]}"; do
  case "$dev" in
    ce1638) opts=(--banked CE-1638); prog=0104; home=00D4 ;;
    ce163f) opts=(--banked CE-163F); prog=0112; home=00E2 ;;
    card)
      if [ -z "${PC1500EXPROM:-}" ]; then
        printf '%-8s %s\n' "$dev" "skipped (no PC1500EXPROM)"
        continue
      fi
      opts=(--card --expansion "$(hostpath "$PC1500EXPROM")"); prog=00C5; home="" ;;
    *) echo "unknown device: $dev" >&2; exit 2 ;;
  esac
  mkdir -p "$WORK/$dev"
  if ! out="$(cd "$WORK/$dev" && "$BUILD" --banks="$dev" "$SRC/banked.c" "$SRC/banked1.c" "$SRC/banked2.c" "$LOAD" 2>&1)"; then
    printf '%-8s %s\n' "$dev" BUILD
    echo "$out" | grep -v '^ *|' | tail -5 | sed 's/^/    /'
    failed=$((failed + 1))
    continue
  fi
  base="$WORK/$dev/banked-$LOAD-pc1500"
  reserve="$(echo "$out" | sed -n 's/.*NEW &\([0-9A-F]*\) (before.*/\1/p')"
  # bank 0 back: the vendor's bank byte, or for the card bank 1's first
  # code byte not there
  if [ -n "$home" ]; then back=("x:$home:00"); else back=("x:00C6:FF"); fi
  if run="$("$PC1500RUNNER" "$(hostpath "$PC1500ROM")" "$(hostpath "$base.bin")" "$LOAD" \
              --ramext 10240 "${opts[@]}" \
              --bank "1:$(hostpath "$base-b1.bin"):$prog" --bank "2:$(hostpath "$base-b2.bin"):$prog" \
              --new "$reserve" --map "$(hostpath "$base.map")" -q \
              u:OK:60000 "${back[@]}" k:ENT w:500 p:77F0:=01 t:CALL k:SPACE "t:&$LOAD" k:ENT \
              u:\>:10000 "${back[@]}" x:_seen:01020201 2>&1)"; then
    sizes="$(echo "$out" | sed -n 's/.*bank \([0-9]*\): .*, \([0-9]*\) bytes.*/b\1 \2/p' | tr '\n' ' ')"
    printf '%-8s ok      fixed %s, %s\n' "$dev" \
      "$(echo "$out" | sed -n 's/.*image: \([0-9]*\) bytes.*/\1/p')" "$sizes"
  else
    printf '%-8s FAIL\n' "$dev"
    echo "$run" | tr -d '\r' | sed 's/^/    /'
    failed=$((failed + 1))
    continue
  fi
  # the card: again from its SD card, the banks in the same file, as
  # LOAD.TXT says (SDLOAD M, keywords.c's segload)
  if [ "$dev" = card ]; then
    if run="$("$PC1500RUNNER" "$(hostpath "$PC1500ROM")" - "$LOAD" \
                --ramext 10240 "${opts[@]}" --sd "$(hostpath "$WORK/$dev")" \
                --new "$reserve" --map "$(hostpath "$base.map")" -q \
                t:SDLOAD k:SPACE 't:M"BANKED.BIN"' k:ENT u:OK:60000 x:00C6:FF k:ENT 'u:>:10000' \
                x:_seen:01020201 2>&1)"; then
      printf '%-8s ok      SDLOAD M: the fixed code and banks from one file\n' "$dev"
    else
      printf '%-8s FAIL    SDLOAD M\n' "$dev"
      echo "$run" | tr -d '\r' | sed 's/^/    /'
      failed=$((failed + 1))
    fi
  fi
done
echo "$failed failed"
exit "$failed"
