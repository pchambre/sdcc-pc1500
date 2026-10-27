#!/bin/bash
# run.sh - the LH5801 port's regression tests: build every tests/*.c (and
# the Blackjack example) with build-lh5801.sh, run each on an emulated
# PC-1500 with pc1500runner, and print pass/fail and a size table.
#
# Paul Chambre, 2026
#
# Usage: run.sh [test ...]      (names without .c; default: all, plus blackjack)
#
# Needs:
#   PC1500RUNNER  pc1500runner, from pc1500emu's tools/runner (a Windows
#                 .exe works from WSL -- its paths are converted with wslpath)
#   PC1500ROM     the PC-1500's ROM1.BIN
#
# Every test is built at 00C5 and run with 16K of RAM at 0000H plus 10K at
# 4800H -- the 28K of contiguous RAM a PC-1500 has with the expansion
# cards -- after NEW &<the program's RAM end>, as a user would. A test's
# script is tests/<name>.script if there is one, else "wait for OK"
# (see tests/check.h). Exit status: the number of failed tests.

set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDCC_ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$SDCC_ROOT/support/scripts/build-lh5801.sh"
BLACKJACK="$SDCC_ROOT/device/examples/lh5801/blackjack/blackjack.c"
LOAD=00C5

: "${PC1500RUNNER:?set PC1500RUNNER to pc1500runner (pc1500emu tools/runner)}"
: "${PC1500ROM:?set PC1500ROM to ROM1.BIN}"

# A path the runner can open: Windows paths for a Windows .exe.
hostpath() {
  case "$PC1500RUNNER" in
    *.exe) wslpath -w "$1" ;;
    *) echo "$1" ;;
  esac
}

if [ $# -gt 0 ]; then
  TESTS=("$@")
else
  TESTS=()
  for f in "$HERE"/tests/*.c; do TESTS+=("$(basename "$f" .c)"); done
  TESTS+=(blackjack)
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

failed=0
printf '%-12s %-6s %8s %8s\n' test result image ram
for t in "${TESTS[@]}"; do
  if [ "$t" = blackjack ]; then
    src="$BLACKJACK"
    script="$HERE/blackjack.script"
  else
    src="$HERE/tests/$t.c"
    script="$HERE/tests/$t.script"
  fi
  mkdir -p "$WORK/$t"
  if ! out="$(cd "$WORK/$t" && "$BUILD" "$src" "$LOAD" 2>&1)"; then
    printf '%-12s %-6s\n' "$t" BUILD
    echo "$out" | grep -v '^ *|' | tail -5 | sed 's/^/    /'
    failed=$((failed + 1))
    continue
  fi
  bin="$WORK/$t/$t-$LOAD-pc1500.bin"
  map="$WORK/$t/$t-$LOAD-pc1500.map"
  reserve="$(echo "$out" | sed -n 's/.*NEW &\([0-9A-F]*\).*/\1/p')"
  image="$(echo "$out" | sed -n 's/.*image: \([0-9]*\) bytes.*/\1/p')"
  ram=$(( 16#$reserve - 16#$LOAD ))
  if [ -f "$script" ]; then
    args=(-f "$(hostpath "$script")")
  else
    args=(u:OK:30000)
  fi
  if run="$("$PC1500RUNNER" "$(hostpath "$PC1500ROM")" "$(hostpath "$bin")" "$LOAD" \
              --ram0000 16384 --ramext 10240 --new "$reserve" --map "$(hostpath "$map")" \
              -q "${args[@]}" 2>&1)"; then
    printf '%-12s %-6s %8s %8s\n' "$t" ok "$image" "$ram"
  else
    printf '%-12s %-6s %8s %8s\n' "$t" FAIL "$image" "$ram"
    echo "$run" | tr -d '\r' | sed 's/^/    /'
    failed=$((failed + 1))
  fi
done
echo "$failed failed"
exit "$failed"
