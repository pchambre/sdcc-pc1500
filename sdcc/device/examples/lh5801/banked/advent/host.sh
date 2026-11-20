#!/bin/bash
# host.sh - builds the port for a PC (HOST) and checks it against
# jwodder's original on the same commands and seed.
#
# Usage: host.sh ORIG_SRC_DIR [commands-file [seed]]
#
# ORIG_SRC_DIR is a checkout of https://github.com/jwodder/advent350's
# c/ directory (all of it; orig/ here has only the data files). It is
# built with SEEDABLE_RNG. Both programs read the commands and their
# output is compared with all whitespace squeezed (the port joins the
# original's 70-column lines). Without a commands file, only builds.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORIG="$1"
CMDS="${2:-}"
SEED="${3:-12345}"
OUT="${OUT:-$(mktemp -d)}"

cc -std=gnu99 -O1 -Wall -Wno-unused-function -DHOST -I"$HERE" -o "$OUT/advent-host" \
   "$HERE"/start.c "$HERE"/main.c "$HERE"/io_host.c "$HERE"/turn.c "$HERE"/motion.c "$HERE"/verbs.c \
   "$HERE"/vocab.c "$HERE"/advdata.c "$HERE"/host_data.c "$HERE"/text_b*.c
cc -std=gnu99 -O1 -w -DSEEDABLE_RNG -o "$OUT/advent-orig" \
   "$ORIG"/advent350.c "$ORIG"/advtext.c "$ORIG"/miscdata.c "$ORIG"/motion.c \
   "$ORIG"/travel.c "$ORIG"/util.c "$ORIG"/verbs.c "$ORIG"/vocab.c
echo "built $OUT/advent-host and $OUT/advent-orig"
[ -n "$CMDS" ] || exit 0

squeeze() { tr -s ' \t\n' ' ' | sed 's/^ //; s/ $//'; }
(cd "$OUT" && ./advent-orig -S "$SEED" < "$CMDS" > orig.txt 2>&1 || true)
(cd "$OUT" && rm -f advent.sav && ./advent-host -S "$SEED" < "$CMDS" > host.txt 2>&1 || true)
squeeze < "$OUT/orig.txt" > "$OUT/orig.sq"
squeeze < "$OUT/host.txt" > "$OUT/host.sq"
if cmp -s "$OUT/orig.sq" "$OUT/host.sq"; then
    echo "same output ($(wc -c < "$OUT/orig.sq") bytes squeezed) for $(wc -l < "$CMDS") commands, seed $SEED"
else
    echo "DIFFERENT output, seed $SEED: $OUT/orig.txt vs $OUT/host.txt"
    cmp "$OUT/orig.sq" "$OUT/host.sq" || true
    exit 1
fi
