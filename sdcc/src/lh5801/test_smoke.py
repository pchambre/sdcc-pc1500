#!/usr/bin/env python3
"""
End-to-end smoke test for the phase-1 LH5801 SDCC backend: compiles
smoke_test/smoke.c with `sdcc -mlh5801`, assembles the result with
sdas/aslh5801, links it with sdld (the shared, port-agnostic linker), and
checks the final linked bytes against expected values.

This only re-checks the static compile+assemble+link output (fast, no
emulator dependency) -- it doesn't re-run the emulated CPU. The semantics
(that this actually executes correctly -- P1 ends up holding 1, and
control returns from main() through the startup code back to BASIC) are
covered by support/regression-lh5801/run.sh, which runs real programs on
pc1500emu; originally verified by hand there on 2026-07-27; see
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
EXIT_ASM = os.path.join(TOP, "device", "lib", "lh5801", "exit.asm")

# Base addresses chosen to fit within a bare PC-1500's built-in 2K RAM
# (4000H-47FFH), clear of the 4000H-40C4H reserve area -- see
# lh5801_setDefaultOptions()'s comment in main.c.
AREA_BASES = {
    "HOME": 0x4200,
    "GSINIT": 0x4210,   # the startup: zero DATA, copy INITIALIZER (47 bytes)
    "GSFINAL": 0x4240,  # save S, call main, back to BASIC (18 bytes)
    "CODE": 0x4260,
    "CONST": 0x4300,
    "INITIALIZER": 0x4310,
    "DATA": 0x4600,
    "SSEG": 0x4610,
    "INITIALIZED": 0x4620,
}

# GSINIT (4210H): psh y / psh u, zero l_DATA (0EH) bytes from s_DATA
# (4600H), copy l_INITIALIZER (0) bytes from 4310H to 4620H. GSFINAL
# (4240H): S into __sdcc_lh5801_sp (460CH), sjp _main, pop u / pop y /
# rtn. CODE (4260H): main's `P1 = 1`, then exit.asm's exit(), which puts S
# back and jumps to GSFINAL's pop u (424DH). HOME is empty.
EXPECTED_IHX = [
    ":20421000FD98FDA848464A0058005A0E948903148B06B50041569E0C48434A1058465A2004",
    ":0F42300068006A00A48903248B054551669E0B24",
    ":12424000FD4884AE460C04AE460DBE4260FD2AFD1A9A66",
    ":13426000B501AE460B9AA5460C08A5460D0AFD4EBA424D67",
]

# _P1 sits at DATA+11, not DATA+0: genLH5801AssemblerStart() reserves 11
# bytes of per-module scratch ahead of any real user DATA variable
# (__lh5801_cmp_scratch/_frame_ptr/_shift_scratch/_cmp_result_msb/
# _cmp_signA/_cmp_signed_lt/_remat0/_remat1 -- see gen.c's own comment
# there for why each one is declared unconditionally up front rather than
# lazily).
EXPECTED_SYMBOLS = {
    "_main": 0x4260,
    "_P1": 0x460B,
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

        # -g: the startup code's s_DATA/l_DATA/... are sdld's own area
        # symbols, undefined until link time. exit.asm holds
        # __sdcc_lh5801_sp, which the startup always references.
        for src, rel in (("smoke.asm", "smoke.rel"), (EXIT_ASM, "exit.rel")):
            rc, out, err = run([ASLH5801, "-lg", "-o", rel, src], cwd=tmpdir)
            if rc != 0 or "error" in (out + err).lower():
                print(f"ASSEMBLE FAILED ({src}):\n{out}\n{err}")
                sys.exit(1)

        link_cmd = [SDLD, "-mjxi"]
        for area, addr in AREA_BASES.items():
            link_cmd += ["-b", f"{area}=0x{addr:04x}"]
        link_cmd += ["-o", "smoke.ihx", "smoke.rel", "exit.rel"]
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
