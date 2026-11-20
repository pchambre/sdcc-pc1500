#!/bin/bash
# gen.sh - makes the port's generated data (advdata.h/.c, text_b*.c,
# *_data.asm, host_data.c) from the original's tables in orig/: dump.c
# (built with the host's cc) prints them as JSON, gen.py writes the
# files. The generated files are kept in the repository, so a build
# needs this only when orig/ or gen.py change.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cc -std=gnu99 -O0 -o "$TMP/dump" "$HERE/dump.c" "$HERE/../orig/advtext.c" \
   "$HERE/../orig/travel.c" "$HERE/../orig/vocab.c" "$HERE/../orig/miscdata.c"
"$TMP/dump" > "$TMP/data.json"
python3 "$HERE/gen.py" "$TMP/data.json" "$HERE/.."
