#!/bin/bash
# make.sh - builds Adventure for the PC-1500 with the internal expansion
# card: the fixed code at 40C5H and banks 0-4, in the directory it's run
# from. MAIN.BIN holds all of it for SDLOAD M (see README.md).
#
# --ce163f: for a PC-1500A (or PC-1501) with a CE-163F instead -- banks
# 0-4 of its RAM, switched at 6800H; main.c and io.c go in bank 4 then
# (ADV_CE163F, advent.h's BANKED_CE), so the fixed part fits the
# PC-1500A's own RAM below 5800H. LOAD.TXT says how to load the banks.
#
# Built --expansion either way: with the SD/BLE expansion module the game
# shows on its Screen pane (BLDISP) and takes its Bluetooth keyboard
# (BLKBD); without one that changes nothing.
#
# Usage: make.sh [--ce163f] [extra build-lh5801.sh options]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDCC_ROOT="$(cd "$HERE/../../../../.." && pwd)"
AS="$SDCC_ROOT/bin/sdaslh5801"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

TARGET=(--banks=card)
if [ "${1:-}" = --ce163f ]; then
  shift
  TARGET=(--banks=ce163f --pc1500a)
  export SDCCFLAGS="${SDCCFLAGS:-} -DADV_CE163F"
fi

RELS=()
for a in "$HERE"/*.asm; do
  n="$(basename "$a" .asm)"
  cp "$a" "$TMP/$n.asm"
  "$AS" -logs "$TMP/$n.asm" >/dev/null
  RELS+=("$TMP/$n.rel")
done

# main.c first: it names the output (MAIN.BIN); the build puts start.c's
# main() first itself
"$SDCC_ROOT/support/scripts/build-lh5801.sh" "${TARGET[@]}" --expansion "$@" \
  "$HERE/main.c" "$HERE/start.c" "$HERE/io.c" "$HERE/savegame.c" "$HERE/advdata.c" \
  "$HERE/turn.c" "$HERE/motion.c" "$HERE/verbs.c" "$HERE/vocab.c" "$HERE"/text_b*.c \
  "${RELS[@]}" 40C5
