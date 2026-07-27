#!/usr/bin/env python3
"""
Assemble two cross-referencing modules (link_test/mod_a.asm, mod_b.asm) with
sdaslh5801, link them with sdld (aslink), and diff the final linked bytes
against expected values -- verifying that sdld, a single generic
port-agnostic linker shared by every sdas target, actually produces correct
output for this port: cross-module .globl symbol resolution, absolute-address
relocation (SJP/STA operands), and PC-relative branch relocation (BCH).

mod_b.asm's _start (linked at 0x4000):
    rie             ; disable interrupts -- there's no ROM/vector table
                     ; in this bare link/run test, so a stray timer or MI
                     ; interrupt would vector into garbage
    ldi s,0x47FF     ; a real stack pointer, so SJP/RTN's push/pop lands in
                     ; actual writable RAM instead of the (read-only, when
                     ; unmapped) high address space S defaults to
    ldi a,5
    sjp _add_one     ; calls into mod_a.asm, linked at 0x400F
    sta (_counter)   ; stores through to mod_a.asm's _counter, at 0x4100
loop:
    bch loop         ; settle into an infinite loop

mod_a.asm's _add_one just does `inc a; rtn`. Linked and actually executed to
completion in pc1500emu (2026-07-26): A ends at 6 (5+1) and _counter (4100H)
reads back 06, confirming both the linked bytes below and the semantics they
encode are correct -- see docs/pc1500_hardware_reference.md in pc1500emu for
that end-to-end run. This test only re-checks the static link output (fast,
no emulator dependency); it doesn't re-run the emulator itself.

Run directly, or via `make check` in this directory. Requires sdaslh5801 and
sdld to already be built (`make` in this directory).
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ASSEMBLER = os.environ.get("ASLH5801", os.path.join(HERE, "sdaslh5801"))
LINKER = os.environ.get("SDLD", os.path.join(HERE, "sdld"))
LINK_TEST_DIR = os.path.join(HERE, "link_test")

# Expected final linked Intel-hex record (single 0x11=17-byte block at 0x4000)
# and the byte-for-byte decode of it, annotated instruction-by-instruction.
EXPECTED_IHX = ":11400000FDBEAA47FFB505BE400FAE41009E02DD9A37"
EXPECTED_BYTES = "FD BE AA 47 FF B5 05 BE 40 0F AE 41 00 9E 02 DD 9A"

# (symbol, expected linked address) -- cross-checked against the .map file
# independently of the raw hex above.
EXPECTED_SYMBOLS = {
    "_start": 0x4000,
    "_add_one": 0x400F,
    "_counter": 0x4100,
}


def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


def main():
    fails = 0

    if not os.path.exists(ASSEMBLER):
        print(f"MISSING: {ASSEMBLER} -- run `make` first")
        sys.exit(1)
    if not os.path.exists(LINKER):
        print(f"MISSING: {LINKER} -- run `make` first")
        sys.exit(1)

    with tempfile.TemporaryDirectory() as tmpdir:
        for name in ("mod_a.asm", "mod_b.asm"):
            with open(os.path.join(LINK_TEST_DIR, name)) as f:
                src = f.read()
            with open(os.path.join(tmpdir, name), "w") as f:
                f.write(src)

        for name in ("mod_a.asm", "mod_b.asm"):
            rc, out, err = run([ASSEMBLER, "-l", "-o", name], cwd=tmpdir)
            if rc != 0 or "error" in (out + err).lower():
                print(f"ASSEMBLE FAILED ({name}):\n{out}\n{err}")
                sys.exit(1)

        rc, out, err = run(
            [LINKER, "-mjxi", "-b", "_CODE=0x4000", "-b", "_DATA=0x4100",
             "-o", "out.ihx", "mod_b.rel", "mod_a.rel"],
            cwd=tmpdir)
        if rc != 0:
            print(f"LINK FAILED:\n{out}\n{err}")
            sys.exit(1)

        with open(os.path.join(tmpdir, "out.ihx")) as f:
            ihx_lines = [l.strip() for l in f if l.strip()]

        data_lines = [l for l in ihx_lines if l[7:9] == "00"]
        if len(data_lines) != 1 or data_lines[0] != EXPECTED_IHX:
            fails += 1
            print(f"FAIL: linked .ihx data record mismatch:\n"
                  f"  expected: {EXPECTED_IHX}\n"
                  f"  got:      {data_lines}")
        else:
            payload = data_lines[0][9:9 + 17 * 2]
            got_bytes = " ".join(payload[i:i + 2].upper() for i in range(0, len(payload), 2))
            if got_bytes != EXPECTED_BYTES:
                fails += 1
                print(f"FAIL: decoded bytes mismatch:\n"
                      f"  expected: {EXPECTED_BYTES}\n"
                      f"  got:      {got_bytes}")

        with open(os.path.join(tmpdir, "out.map")) as f:
            map_text = f.read()
        # Not line-anchored: the symbol table packs two "addr  name" pairs
        # per line, separated by a "|", when there's room (e.g.
        # "00004000  _start    |    0000400F  _add_one").
        sym_re = re.compile(r"([0-9A-Fa-f]{8})\s+(\S+)")
        map_syms = {name: int(addr, 16) for addr, name in sym_re.findall(map_text)}
        for name, expected_addr in EXPECTED_SYMBOLS.items():
            got_addr = map_syms.get(name)
            if got_addr != expected_addr:
                fails += 1
                print(f"FAIL: symbol {name!r}: expected {expected_addr:04X}H, "
                      f"got {'(missing)' if got_addr is None else f'{got_addr:04X}H'}")

    total = 2 + len(EXPECTED_SYMBOLS)
    print(f"\n{total - fails}/{total} link checks passed.")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
