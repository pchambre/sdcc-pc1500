#!/bin/bash
# lcdtest.sh - plays the same game on the emulated PC-1500 (make.sh's
# output: the card build, or a PC-1500A with a CE-163F for make.sh
# --ce163f's, as its LOAD.TXT says) and on a PC build of the very same code
# (io.c included; test/host_lcd.c stands in for the display and keys),
# and checks that the PC-1500 shows every display the PC build did. The
# PC-1500's generator gets the PC build's seed through the map.
#
# Usage: lcdtest.sh BUILD_DIR commands-file [seed]
#   BUILD_DIR: where make.sh was run (main-40C5-pc1500*.bin, .map,
#   LOAD.TXT). Needs PC1500RUNNER, PC1500ROM and PC1500EXPROM as
#   support/regression-lh5801/banked.sh does.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADV="$HERE/.."
B="$(cd "$1" && pwd)"
CMDS="$2"
SEED="${3:-12345}"
: "${PC1500RUNNER:?}" "${PC1500ROM:?}" "${PC1500EXPROM:?}"
OUT="${OUT:-$(mktemp -d)}"

hostpath() {
  case "$PC1500RUNNER" in
    *.exe) wslpath -w "$1" ;;
    *) echo "$1" ;;
  esac
}

cc -std=gnu99 -O1 -w -DHOST -I"$HERE/include" -I"$ADV" -o "$OUT/advent-lcd" \
   "$ADV"/start.c "$ADV"/main.c "$ADV"/io.c "$ADV"/savegame.c "$ADV"/turn.c "$ADV"/motion.c "$ADV"/verbs.c \
   "$ADV"/vocab.c "$ADV"/advdata.c "$ADV"/host_data.c "$ADV"/text_b*.c "$HERE"/host_lcd.c
tr 'a-z' 'A-Z' < "$CMDS" > "$OUT/keys.txt"
"$OUT/advent-lcd" -S "$SEED" "$OUT/keys.txt" > "$OUT/lcd.log"
python3 "$HERE/lcdtest.py" "$OUT/lcd.log" "$SEED" > "$OUT/script.txt"
echo "$(wc -l < "$OUT/lcd.log") displays, $(wc -l < "$CMDS") commands; script $OUT/script.txt"

NEW="$(sed -n 's/^  NEW &\([0-9A-F]*\)$/\1/p' "$B/LOAD.TXT")"
if grep -q 'banked code for ce163f' "$B/LOAD.TXT"; then
  MACHINE=(--pc1500a --banked CE-163F); AT=0112
else
  MACHINE=(--ramext 10240 --card); AT=00C5
fi
BANKS=()
for n in 0 1 2 3 4; do
  BANKS+=(--bank "$n:$(hostpath "$B/main-40C5-pc1500-b$n.bin"):$AT")
done
"$PC1500RUNNER" "$(hostpath "$PC1500ROM")" "$(hostpath "$B/main-40C5-pc1500.bin")" 40C5 \
  "${MACHINE[@]}" --expansion "$(hostpath "$PC1500EXPROM")" "${BANKS[@]}" \
  --new "$NEW" --map "$(hostpath "$B/main-40C5-pc1500.map")" -q \
  -f "$(hostpath "$OUT/script.txt")"
