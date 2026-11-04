#!/bin/bash
# profile.sh - build one regression-suite case the way the suite does, run
# it on pc1500sim with a profile, and show what its stack slots and saved
# frames cost (slot_cost.py).
#
# Paul Chambre, 2026
#
# Usage: profile.sh <case.c> [outdir]
#   case.c   a file from support/regression/cases (the suite builds those;
#            its gen/lh5801 objects -- testfwk, support, fwk.lib -- must exist)
#   outdir   where the build goes (default: ./profile-out)
# Environment: SDCCFLAGS (extra flags, e.g. --opt-code-speed),
#              PC1500SIM (default: pc1500sim on PATH, built with -p support)
#
# The compiler marks slot copies with comments when LH5801_MARK is set
# (gen.c's slotSync()); the code itself is the same.

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$HERE/../.." && pwd)"
G="$R/support/regression/gen/lh5801"
SIM="${PC1500SIM:-pc1500sim}"
src="$(realpath "$1")"
OUT="${2:-profile-out}"; mkdir -p "$OUT"; cd "$OUT"
n=$(basename "$src" .c)
FL="-mlh5801 --less-pedantic -DSTACK_SIZE=4096 -I$R/support/regression -I$R/device/include \
    -I$R/device/include/lh5801 -I$R/support/regression/fwk/include -I$R/support/regression/tests"
LH5801_MARK=1 "$R/bin/sdcc" $FL ${SDCCFLAGS:-} -c "$src" -o "$n.rel"
"$R/support/scripts/build-lh5801.sh" "$n.rel" "$G/testfwk.rel" "$G/support.rel" "$G/fwk.lib" 0100 >/dev/null 2>&1
"$SIM" -p "$n.prof" "$n-0100-pc1500.bin" 0100 | grep -a "Summary\|clks"
python3 "$HERE/slot_cost.py" . "$n"
