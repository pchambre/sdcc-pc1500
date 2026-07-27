#!/usr/bin/env python3
"""
Assemble every documented LH5801 addressing-mode variant with sdaslh5801 and
diff the emitted bytes against expected values transcribed independently
(a fresh re-read of pc1500emu's docs/lh5801_opcode_reference.md, not
copy-pasted from m5801pst.c) -- an independent oracle to catch transcription
mistakes in either the table or the encoder logic. Also separately verifies
the direction-selecting branch opcodes' displacement math (see m5801mch.c's
S_TYPBRA comment) against explicit hand-computed forward/backward/self
branch cases.

Run directly, or via `make check` in this directory. Requires sdaslh5801 to
already be built (`make` in this directory).
"""
import re
import subprocess
import sys
import os
import tempfile

ASSEMBLER = os.environ.get(
    "ASLH5801", os.path.join(os.path.dirname(os.path.abspath(__file__)), "sdaslh5801"))

# (asm operand line, expected hex bytes as a string like "02" or "A3 12 34")
CASES = [
    # ADC
    ("adc xl", "02"), ("adc yl", "12"), ("adc ul", "22"),
    ("adc xh", "82"), ("adc yh", "92"), ("adc uh", "A2"),
    ("adc (x)", "03"), ("adc (y)", "13"), ("adc (u)", "23"),
    ("adc (0x1234)", "A3 12 34"),
    ("adc #(x)", "FD 03"), ("adc #(y)", "FD 13"), ("adc #(u)", "FD 23"),
    ("adc #(0x1234)", "FD A3 12 34"),
    # SBC
    ("sbc xl", "00"), ("sbc yl", "10"), ("sbc ul", "20"),
    ("sbc xh", "80"), ("sbc yh", "90"), ("sbc uh", "A0"),
    ("sbc (x)", "01"), ("sbc (y)", "11"), ("sbc (u)", "21"),
    ("sbc (0x1234)", "A1 12 34"),
    ("sbc #(x)", "FD 01"), ("sbc #(y)", "FD 11"), ("sbc #(u)", "FD 21"),
    ("sbc #(0x1234)", "FD A1 12 34"),
    # CPA
    ("cpa xl", "06"), ("cpa yl", "16"), ("cpa ul", "26"),
    ("cpa xh", "86"), ("cpa yh", "96"), ("cpa uh", "A6"),
    ("cpa (x)", "07"), ("cpa (y)", "17"), ("cpa (u)", "27"),
    ("cpa (0x1234)", "A7 12 34"),
    ("cpa #(x)", "FD 07"), ("cpa #(y)", "FD 17"), ("cpa #(u)", "FD 27"),
    ("cpa #(0x1234)", "FD A7 12 34"),
    # LDA
    ("lda xl", "04"), ("lda yl", "14"), ("lda ul", "24"),
    ("lda xh", "84"), ("lda yh", "94"), ("lda uh", "A4"),
    ("lda (x)", "05"), ("lda (y)", "15"), ("lda (u)", "25"),
    ("lda (0x1234)", "A5 12 34"),
    ("lda #(x)", "FD 05"), ("lda #(y)", "FD 15"), ("lda #(u)", "FD 25"),
    ("lda #(0x1234)", "FD A5 12 34"),
    # STA
    ("sta xl", "0A"), ("sta yl", "1A"), ("sta ul", "2A"),
    ("sta xh", "08"), ("sta yh", "18"), ("sta uh", "28"),
    ("sta (x)", "0E"), ("sta (y)", "1E"), ("sta (u)", "2E"),
    ("sta (0x1234)", "AE 12 34"),
    ("sta #(x)", "FD 0E"), ("sta #(y)", "FD 1E"), ("sta #(u)", "FD 2E"),
    ("sta #(0x1234)", "FD AE 12 34"),
    # AND/ORA/EOR/BIT
    ("and (x)", "09"), ("and (y)", "19"), ("and (u)", "29"),
    ("and (0x1234)", "A9 12 34"),
    ("and #(x)", "FD 09"), ("and #(y)", "FD 19"), ("and #(u)", "FD 29"),
    ("and #(0x1234)", "FD A9 12 34"),
    ("ora (x)", "0B"), ("ora (y)", "1B"), ("ora (u)", "2B"),
    ("ora (0x1234)", "AB 12 34"),
    ("ora #(x)", "FD 0B"), ("ora #(y)", "FD 1B"), ("ora #(u)", "FD 2B"),
    ("ora #(0x1234)", "FD AB 12 34"),
    ("eor (x)", "0D"), ("eor (y)", "1D"), ("eor (u)", "2D"),
    ("eor (0x1234)", "AD 12 34"),
    ("eor #(x)", "FD 0D"), ("eor #(y)", "FD 1D"), ("eor #(u)", "FD 2D"),
    ("eor #(0x1234)", "FD AD 12 34"),
    ("bit (x)", "0F"), ("bit (y)", "1F"), ("bit (u)", "2F"),
    ("bit (0x1234)", "AF 12 34"),
    ("bit #(x)", "FD 0F"), ("bit #(y)", "FD 1F"), ("bit #(u)", "FD 2F"),
    ("bit #(0x1234)", "FD AF 12 34"),
    # DCA/DCS
    ("dca (x)", "8C"), ("dca (y)", "9C"), ("dca (u)", "AC"),
    ("dca #(x)", "FD 8C"), ("dca #(y)", "FD 9C"), ("dca #(u)", "FD AC"),
    ("dcs (x)", "0C"), ("dcs (y)", "1C"), ("dcs (u)", "2C"),
    ("dcs #(x)", "FD 0C"), ("dcs #(y)", "FD 1C"), ("dcs #(u)", "FD 2C"),
    # ADI/ANI/ORI/BII
    ("adi a,0x11", "B3 11"), ("adi (x),0x11", "4F 11"), ("adi (y),0x11", "5F 11"),
    ("adi (u),0x11", "6F 11"), ("adi (0x1234),0x11", "EF 12 34 11"),
    ("adi #(x),0x11", "FD 4F 11"), ("adi #(y),0x11", "FD 5F 11"),
    ("adi #(u),0x11", "FD 6F 11"), ("adi #(0x1234),0x11", "FD EF 12 34 11"),
    ("ani a,0x11", "B9 11"), ("ani (x),0x11", "49 11"), ("ani (y),0x11", "59 11"),
    ("ani (u),0x11", "69 11"), ("ani (0x1234),0x11", "E9 12 34 11"),
    ("ani #(x),0x11", "FD 49 11"), ("ani #(y),0x11", "FD 59 11"),
    ("ani #(u),0x11", "FD 69 11"), ("ani #(0x1234),0x11", "FD E9 12 34 11"),
    ("ori a,0x11", "BB 11"), ("ori (x),0x11", "4B 11"), ("ori (y),0x11", "5B 11"),
    ("ori (u),0x11", "6B 11"), ("ori (0x1234),0x11", "EB 12 34 11"),
    ("ori #(x),0x11", "FD 4B 11"), ("ori #(y),0x11", "FD 5B 11"),
    ("ori #(u),0x11", "FD 6B 11"), ("ori #(0x1234),0x11", "FD EB 12 34 11"),
    ("bii a,0x11", "BF 11"), ("bii (x),0x11", "4D 11"), ("bii (y),0x11", "5D 11"),
    ("bii (u),0x11", "6D 11"), ("bii (0x1234),0x11", "ED 12 34 11"),
    ("bii #(x),0x11", "FD 4D 11"), ("bii #(y),0x11", "FD 5D 11"),
    ("bii #(u),0x11", "FD 6D 11"), ("bii #(0x1234),0x11", "FD ED 12 34 11"),
    # CPI
    ("cpi a,0x11", "B7 11"), ("cpi xl,0x11", "4E 11"), ("cpi yl,0x11", "5E 11"),
    ("cpi ul,0x11", "6E 11"), ("cpi xh,0x11", "4C 11"), ("cpi yh,0x11", "5C 11"),
    ("cpi uh,0x11", "6C 11"),
    # SBI/EAI
    ("sbi a,0x11", "B1 11"), ("eai a,0x11", "BD 11"),
    # INC/DEC
    ("inc a", "DD"), ("inc xl", "40"), ("inc yl", "50"), ("inc ul", "60"),
    ("inc xh", "FD 40"), ("inc yh", "FD 50"), ("inc uh", "FD 60"),
    ("inc x", "44"), ("inc y", "54"), ("inc u", "64"),
    ("dec a", "DF"), ("dec xl", "42"), ("dec yl", "52"), ("dec ul", "62"),
    ("dec xh", "FD 42"), ("dec yh", "FD 52"), ("dec uh", "FD 62"),
    ("dec x", "46"), ("dec y", "56"), ("dec u", "66"),
    # DRL/DRR
    ("drl (x)", "D7"), ("drl #(x)", "FD D7"),
    ("drr (x)", "D3"), ("drr #(x)", "FD D3"),
    # misc single-opcode / inherent
    ("hlt", "FD B1"), ("ita", "FD BA"), ("jmp 0x1234", "BA 12 34"),
    ("nop", "38"), ("off", "FD 4C"),
    ("rdp", "FD C0"), ("rec", "F9"), ("rie", "FD BE"),
    ("rol", "DB"), ("ror", "D1"), ("rpu", "E3"), ("rpv", "B8"),
    ("rti", "8A"), ("rtn", "9A"), ("sdp", "FD C1"), ("sec", "FB"),
    ("shl", "D9"), ("shr", "D5"), ("sie", "FD 81"), ("sjp 0x1234", "BE 12 34"),
    ("spu", "E1"), ("spv", "A8"), ("tin", "F5"), ("tta", "FD AA"),
    ("aex", "F1"), ("am0", "FD CE"), ("am1", "FD DE"), ("atp", "FD CC"),
    ("att", "FD EC"), ("cdv", "FD 8E"), ("cin", "F7"),
    # LDI
    ("ldi xl,0x11", "4A 11"), ("ldi yl,0x11", "5A 11"), ("ldi ul,0x11", "6A 11"),
    ("ldi xh,0x11", "48 11"), ("ldi yh,0x11", "58 11"), ("ldi uh,0x11", "68 11"),
    ("ldi a,0x11", "B5 11"), ("ldi s,0x1234", "AA 12 34"),
    # LDE/LIN/SDE/SIN
    ("lde x", "47"), ("lde y", "57"), ("lde u", "67"),
    ("lin x", "45"), ("lin y", "55"), ("lin u", "65"),
    ("sde x", "43"), ("sde y", "53"), ("sde u", "63"),
    ("sin x", "41"), ("sin y", "51"), ("sin u", "61"),
    # LDX/STX
    ("ldx x", "FD 08"), ("ldx y", "FD 18"), ("ldx u", "FD 28"),
    ("ldx s", "FD 48"), ("ldx p", "FD 58"),
    ("stx x", "FD 4A"), ("stx y", "FD 5A"), ("stx u", "FD 6A"),
    ("stx s", "FD 4E"), ("stx p", "FD 5E"),
    # ADR
    ("adr x", "FD CA"), ("adr y", "FD DA"), ("adr u", "FD EA"),
    # PSH/POP
    ("psh a", "FD C8"), ("psh x", "FD 88"), ("psh y", "FD 98"), ("psh u", "FD A8"),
    ("pop a", "FD 8A"), ("pop x", "FD 0A"), ("pop y", "FD 1A"), ("pop u", "FD 2A"),
    # LOP
    ("lop ul,5", "88 05"),
    # VCS/VCR/VMJ + conditional vector calls
    ("vcs 0xC0", "C3 C0"), ("vcr 0xC0", "C1 C0"),
    ("vmj 0xC2", "CD C2"), ("vvs 0xC2", "CF C2"), ("vzs 0xC2", "CB C2"),
    ("vzr 0xC2", "C9 C2"), ("vhr 0xC2", "C5 C2"), ("vhs 0xC2", "C7 C2"),
    # VEJ
    ("vej 0xC0", "C0"), ("vej 0xF6", "F6"),
]

# Explicit forward/backward/self-branch cases with hand-computed expected
# opcode+displacement, since the direction-selecting opcode choice (see
# m5801mch.c's S_TYPBRA comment) is the one place this port's encoding
# logic genuinely diverges from a normal single-opcode signed-displacement
# relative branch.
BRANCH_ASM = """\
	.area CODE (ABS)
	.org 0x1000
p1:
	bch p2
	bch p3
p2:
	nop
	nop
p3:
	nop
p4:
	bch p4
	bch p1
	bzs p2
"""
BRANCH_EXPECTED = [
    ("bch p2 @1000", "8E 02"),         # target 1004, A+2=1002, diff=+2
    ("bch p3 @1002", "8E 02"),         # target 1006, A+2=1004, diff=+2
    ("bch p4 @1007 (self)", "9E 02"),  # target 1007, A+2=1009, diff=-2
    ("bch p1 @1009", "9E 0B"),         # target 1000, A+2=100B, diff=-0x0B
    ("bzs p2 @100B", "9B 09"),         # target 1004, A+2=100D, diff=-9
]

LST_LINE_RE = re.compile(
    r"^\s*([0-9A-Fa-f]{4})\s+((?:[0-9A-Fa-f]{2}\s*)+?)\s+\d+\s+\t(\S.*)$",
    re.MULTILINE)


def norm(hexstr):
    return " ".join(b.upper() for b in hexstr.split())


def assemble(tmpdir, source_text):
    asm_path = os.path.join(tmpdir, "t.asm")
    with open(asm_path, "w") as f:
        f.write(source_text)
    r = subprocess.run([ASSEMBLER, "-l", "-o", asm_path], cwd=tmpdir,
                        capture_output=True, text=True)
    if r.returncode != 0 or "error" in (r.stdout + r.stderr).lower():
        print("ASSEMBLE FAILED:")
        print(r.stdout)
        print(r.stderr)
        sys.exit(1)
    with open(os.path.join(tmpdir, "t.lst")) as f:
        return f.read()


def main():
    fails = 0
    total = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        source = "".join("\t" + asmline + "\n" for asmline, _ in CASES)
        lst = assemble(tmpdir, source)
        got = [(addr, norm(hexpart), src.strip())
               for addr, hexpart, src in LST_LINE_RE.findall(lst)]
        if len(got) != len(CASES):
            print(f"MISMATCH IN LINE COUNT: expected {len(CASES)}, got {len(got)}")
            sys.exit(1)
        for (asmline, expected), (addr, actual, src) in zip(CASES, got):
            total += 1
            if norm(expected) != actual:
                fails += 1
                print(f"FAIL: {asmline!r}: expected [{norm(expected)}] got [{actual}] (src={src!r})")

    with tempfile.TemporaryDirectory() as tmpdir:
        lst = assemble(tmpdir, BRANCH_ASM)
        branch_got = [(addr, norm(hexpart), src.strip())
                      for addr, hexpart, src in LST_LINE_RE.findall(lst)
                      if src.strip().startswith("b")]
        if len(branch_got) != len(BRANCH_EXPECTED):
            print(f"BRANCH MISMATCH IN LINE COUNT: expected {len(BRANCH_EXPECTED)}, got {len(branch_got)}")
            sys.exit(1)
        for (label, expected), (addr, actual, src) in zip(BRANCH_EXPECTED, branch_got):
            total += 1
            if norm(expected) != actual:
                fails += 1
                print(f"FAIL: {label}: expected [{norm(expected)}] got [{actual}] (addr={addr} src={src!r})")

    print(f"\n{total - fails}/{total} cases passed.")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
