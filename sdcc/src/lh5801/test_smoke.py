#!/usr/bin/env python3
"""
End-to-end smoke test for the phase-1 LH5801 SDCC backend: compiles
smoke_test/smoke.c with `sdcc -mlh5801`, assembles the result with
sdas/aslh5801, links it with sdld (the shared, port-agnostic linker), and
checks the final linked bytes against expected values.

This only re-checks the static compile+assemble+link output (fast, no
emulator dependency) -- it doesn't re-run the emulated CPU. The semantics
(that this actually executes correctly -- P1 ends up holding 1, and
control correctly returns from main() into the startup code's forever
loop) were verified by hand in pc1500emu on 2026-07-27; see
docs/lh5801_hardware_reference.md in pc1500emu, and
lh5801-pc1500-project-plan / lh5801-sdas-no-dead-code-elimination in
project memory for the wider context.

Uses --c1mode (already-preprocessed input) rather than a normal
compile, since it doesn't require sdcpp to be built -- smoke.c has no
preprocessor directives, so feeding it directly works fine.

Run directly (needs `sdcc`, `sdaslh5801`, `sdld` already built -- `make`
from the sdcc top level with the lh5801 port enabled).
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
SDCC = os.path.abspath(os.environ.get("SDCC", os.path.join(TOP, "bin", "sdcc")))
ASLH5801 = os.path.abspath(os.environ.get("ASLH5801", os.path.join(TOP, "bin", "sdaslh5801")))
SDLD = os.path.abspath(os.environ.get("SDLD", os.path.join(TOP, "bin", "sdld")))
SMOKE_C = os.path.join(HERE, "smoke_test", "smoke.c")

# Base addresses chosen to fit within a bare PC-1500's built-in 2K RAM
# (4000H-47FFH), clear of the 4000H-40C4H reserve area -- see
# lh5801_setDefaultOptions()'s comment in main.c.
AREA_BASES = {
    "HOME": 0x4200,
    "GSINIT": 0x4210,
    "GSFINAL": 0x4230,
    "CODE": 0x4240,
    "CONST": 0x4300,
    "INITIALIZER": 0x4310,
    "DATA": 0x4600,
    "SSEG": 0x4610,
    "INITIALIZED": 0x4620,
}

EXPECTED_IHX = [
    ":08421000AA47FFBE42409E02D6",
    ":03423000BA42008F",
    ":03420000BE42407B",
    ":06424000B501AE46009A34",
]

EXPECTED_SYMBOLS = {
    "_main": 0x4240,
    "_P1": 0x4600,
}


def run(cmd, cwd=None, stdin_path=None):
    stdin = open(stdin_path, "rb") if stdin_path else None
    try:
        r = subprocess.run(cmd, cwd=cwd, stdin=stdin, capture_output=True, text=True)
    finally:
        if stdin:
            stdin.close()
    return r.returncode, r.stdout, r.stderr


def main():
    fails = 0

    for name, path in (("sdcc", SDCC), ("sdaslh5801", ASLH5801), ("sdld", SDLD)):
        if not os.path.exists(path):
            print(f"MISSING: {name} at {path} -- build it first (`make` from the sdcc top level)")
            sys.exit(1)

    with tempfile.TemporaryDirectory() as tmpdir:
        asm_path = os.path.join(tmpdir, "smoke.asm")
        rc, out, err = run([SDCC, "-mlh5801", "--c1mode", "-o", asm_path], stdin_path=SMOKE_C)
        if rc != 0:
            print(f"COMPILE FAILED:\n{out}\n{err}")
            sys.exit(1)

        rc, out, err = run([ASLH5801, "-l", "-o", "smoke.asm"], cwd=tmpdir)
        if rc != 0 or "error" in (out + err).lower():
            print(f"ASSEMBLE FAILED:\n{out}\n{err}")
            sys.exit(1)

        link_cmd = [SDLD, "-mjxi"]
        for area, addr in AREA_BASES.items():
            link_cmd += ["-b", f"{area}=0x{addr:04x}"]
        link_cmd += ["-o", "smoke.ihx", "smoke.rel"]
        rc, out, err = run(link_cmd, cwd=tmpdir)
        if rc != 0:
            print(f"LINK FAILED:\n{out}\n{err}")
            sys.exit(1)

        with open(os.path.join(tmpdir, "smoke.ihx")) as f:
            ihx_lines = [l.strip() for l in f if l.strip()]
        data_lines = [l for l in ihx_lines if l[7:9] == "00"]
        if data_lines != EXPECTED_IHX:
            fails += 1
            print(f"FAIL: linked .ihx data records mismatch:\n"
                  f"  expected: {EXPECTED_IHX}\n"
                  f"  got:      {data_lines}")

        with open(os.path.join(tmpdir, "smoke.map")) as f:
            map_text = f.read()
        sym_re = re.compile(r"([0-9A-Fa-f]{8})\s+(\S+)")
        map_syms = {name: int(addr, 16) for addr, name in sym_re.findall(map_text)}
        for name, expected_addr in EXPECTED_SYMBOLS.items():
            got_addr = map_syms.get(name)
            if got_addr != expected_addr:
                fails += 1
                print(f"FAIL: symbol {name!r}: expected {expected_addr:04X}H, "
                      f"got {'(missing)' if got_addr is None else f'{got_addr:04X}H'}")

    total = 1 + len(EXPECTED_SYMBOLS)
    print(f"\n{total - fails}/{total} smoke checks passed.")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
