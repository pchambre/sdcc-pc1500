#!/usr/bin/env python3
"""gen.py - turns the original game's tables (dump.c's JSON) into the
PC-1500 port's data: the text, travel table and vocabulary as assembly
for the banks, the small tables the fixed code needs as C, and the same
data as plain C for the host build (host.sh).

Usage: gen.py data.json outdir

Text: every message (rmsg, longdesc, shortdesc, itemDesc, the class
ranks, and the port's own messages below) gets a global number. The
messages are packed in that order into the text banks (TEXT_BANKS), each
bank holding an index of 2-byte offsets (FFFFH: no message) for its own
range of numbers, then the bytes, each message ending in 00H. A bank's
printer, text_b<n>.c, is generated too.

The original's line breaks were for a 70-column terminal; a message
becomes one paragraph (its lines joined with spaces), keeping a line
break only before a line that starts with blanks or after an empty line.
Runs of spaces become one.
"""

import json
import os
import re
import sys

# bank -> bytes the bank's text may use (data and index; the printer is
# about 140 bytes more). A bank's code goes in 00C6H-3FFFH on the card
# (16186 bytes) and 0113H-3FFFH on a CE-163F (16109: its vendor's
# routines keep the first 112H bytes of every bank).
TEXT_BANKS = [(1, 15900), (2, 15900), (3, 15900)]
TRAVEL_BANK = 4     # with motion.c
VOCAB_BANK = 0      # with vocab.c

# The port's own messages: the original printf()s, cut where a number or
# the player's word goes in. Not joined or squeezed: kept as written.
EXTRA = [
    ("M_DWARVES1", "There are "),
    ("M_DWARVES2", " threatening little dwarves in the room with you."),
    ("M_KNIVES", " of them throw knives at you!"),
    ("M_GETYOU", " of them get you!"),
    ("M_HINT1", "I am prepared to give you a hint, but it will cost you "),
    ("M_POINTS", " points."),
    ("M_ISEENO", "I see no "),
    ("M_HERE", " here."),
    ("M_WHATDO", "What do you want to do with the "),
    ("M_QMARK", "?"),
    ("M_WHAT", " what?"),
    ("M_OKAY", "Okay, \""),
    ("M_QUOTE", "\"."),
    ("M_ANSWER", "Please answer the question."),
    ("M_SCORED1", "You scored "),
    ("M_SCORED2", " out of a possible 350 using "),
    ("M_TURN", " turn"),
    ("M_S", "s"),
    ("M_DOT", "."),
    ("M_NEXT1", "To achieve the next higher rating, you need "),
    ("M_MOREPT", " more point"),
    ("M_NEATTRICK", "To achieve the next higher rating would be a neat trick!"),
    ("M_CONGRATS", "Congratulations!!"),
    ("M_OFFSCALE", "You just went off my scale!!"),
    ("M_WOULDSCORE", "If you were to quit now, you would score "),
    ("M_OF350", " out of a possible 350."),
    ("M_OPENALL", "Colossal Cave is open all day, every day."),
    ("M_SUSPEND", "I can suspend your adventure for you so that you can resume later."),
    ("M_SAVED", "Your adventure is saved in the expansion card's RAM. CALL the game again to resume it."),
    ("M_ABANDON", "To resume an earlier Adventure, you must abandon the current one."),
    ("M_NOSAVE", "There is no saved adventure."),
    ("M_RESTORED", "Your saved adventure is restored."),
    ("M_RESUMEQ", "Resume your saved adventure?"),
    ("M_BUG", "Fatal error, see source code for interpretation. Probable cause: erroneous info in database. Error code = "),
    ("M_TITLE", "COLOSSAL CAVE ADVENTURE"),
    ("M_PRESSKEY", "Press a key to start"),
]


def norm(s):
    s = s.replace("\t", "        ")
    lines = s.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    out = ""
    prev_blank = False
    for i, line in enumerate(lines):
        if line.strip() == "":
            prev_blank = True
            continue
        if i > 0 and out:
            out += "\n" if (line.startswith(" ") or prev_blank) else " "
        out += line.strip()
        prev_blank = False
    return re.sub(" +", " ", out)


def messages(d):
    """[(name or None, text or None)] in global-number order, and the
    #defines for where each group starts."""
    msgs, defs = [], []

    def group(name, texts):
        defs.append((name, len(msgs)))
        msgs.extend(texts)

    group("T_RMSG", [s and norm(s) for s in d["rmsg"]])
    group("T_LONG", [s and norm(s) for s in d["longdesc"]])
    group("T_SHORT", [s and norm(s) for s in d["shortdesc"]])
    group("T_ITEM", [s and norm(s) for row in d["itemDesc"] for s in row])
    group("T_CLASS", [norm(r) for _, r in d["classes"]])
    for name, text in EXTRA:
        defs.append((name, len(msgs)))
        msgs.append(text)
    defs.append(("T_COUNT", len(msgs)))
    return msgs, defs


def text_banks(msgs):
    """[(bank, first, end, index, data)]"""
    out, n = [], 0
    for bank, cap in TEXT_BANKS:
        first, data, index = n, bytearray(), []
        while n < len(msgs):
            t = msgs[n]
            b = t.encode("ascii") + b"\0" if t is not None else b""
            if 2 * (len(index) + 1) + len(data) + len(b) > cap:
                break
            index.append(len(data) if t is not None else 0xFFFF)
            data += b
            n += 1
        out.append((bank, first, n, index, bytes(data)))
    if n < len(msgs):
        sys.exit(f"gen.py: the text doesn't fit the text banks ({len(msgs) - n} messages left)")
    return out


def travel_table(d):
    """Per location an offset into the rows; each row: condition (2 bytes,
    the original's L / 1000), destination (2 bytes, L % 1000), then its
    motion words, the last with bit 7 set. FFH ends a location's rows."""
    offs, data = [], bytearray()
    for rows in d["travel"]:
        offs.append(len(data))
        for row in rows:
            ll, motions = row[0], row[1:]
            c, dest = ll // 1000, ll % 1000
            assert motions and all(0 < m < 128 for m in motions)
            data += bytes([c >> 8, c & 255, dest >> 8, dest & 255])
            data += bytes(motions[:-1]) + bytes([motions[-1] | 0x80])
        data.append(0xFF)
    return offs, bytes(data)


def vocab_table(d):
    """Per word 5 bytes (NUL-padded) and the two values, 2 bytes each."""
    data = bytearray()
    words = [w for w, _, _ in d["vocabulary"]]
    assert words == sorted(words), "vocabulary not sorted"
    for w, v1, v2 in d["vocabulary"]:
        data += w.encode("ascii").ljust(5, b"\0")[:5]
        data += bytes([v1 >> 8, v1 & 255, v2 >> 8, v2 & 255])
    return bytes(data)


def asm_bytes(f, data, per=16):
    for i in range(0, len(data), per):
        f.write("\t.db\t" + ",".join("0x%02X" % b for b in data[i:i + per]) + "\n")


def c_bytes(f, name, data):
    f.write(f"const unsigned char {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), 16):
        f.write("  " + ",".join("%d" % b for b in data[i:i + 16]) + ",\n")
    f.write("};\n")


def be16(vals):
    return b"".join(bytes([v >> 8, v & 255]) for v in vals)


HEAD = "/* Generated by gen/gen.py from the original game's tables -- don't edit. */\n"


def main():
    d = json.load(open(sys.argv[1]))
    outdir = sys.argv[2]
    msgs, defs = messages(d)
    banks = text_banks(msgs)
    toffs, tdata = travel_table(d)
    vdata = vocab_table(d)

    # flags in cond[]: the original's bits 0-9, then these
    forced = [l > 0 and len(rows) > 0 and len(rows[0]) > 1 and rows[0][1] == 1
              for l, rows in enumerate(d["travel"])]
    cond = [c | (1024 if forced[l] else 0) | (2048 if d["shortdesc"][l] else 0)
            for l, c in enumerate(d["cond"])]
    assert all(c < 4096 for c in d["cond"])

    with open(os.path.join(outdir, "advdata.h"), "w") as f:
        f.write(HEAD + "#ifndef ADVDATA_H\n#define ADVDATA_H\n\n")
        for name, v in defs:
            f.write(f"#define {name} {v}\n")
        f.write("\n/* cond[] flags besides the original's bits 0-9 */\n")
        f.write("#define COND_FORCED 1024\n#define COND_SHORT 2048\n\n")
        f.write(f"#define TRAVEL_BANK {TRAVEL_BANK}\n#define VOCAB_BANK {VOCAB_BANK}\n")
        f.write(f"#define WORDQTY {len(d['vocabulary'])}\n")
        f.write(f"#define NCLASSES {len(d['classes'])}\n\n")
        f.write("/* the text banks: numbers [first, end) of each */\n")
        for bank, first, end, _, _ in banks:
            f.write(f"#define TEXT_B{bank}_FIRST {first}\n#define TEXT_B{bank}_END {end}\n")
            f.write(f"void tsay_b{bank} (unsigned int n) BANKED;\n")
        f.write("\n#define TSAY_DISPATCH(n) \\\n")
        for bank, first, end, _, _ in banks:
            f.write(f"    if ((n) < TEXT_B{bank}_END) tsay_b{bank} (n); else \\\n")
        f.write("    {}\n\n")
        f.write("extern const unsigned int cond[141];\n")
        f.write("extern const unsigned char actspk[32];\n")
        f.write("extern const unsigned char hints[10][4];\n")
        f.write(f"extern const unsigned int classes[{len(d['classes'])}];\n")
        f.write("\n#endif\n")

    with open(os.path.join(outdir, "advdata.c"), "w") as f:
        f.write(HEAD + "#include \"advent.h\"\n\n")
        f.write("const unsigned int cond[141] = {\n  " +
                ",".join(str(c) for c in cond) + "\n};\n")
        f.write("const unsigned char actspk[32] = {\n  " +
                ",".join(str(c) for c in d["actspk"]) + "\n};\n")
        f.write("/* the original's 9999 (never offered) as 255 */\n")
        f.write("const unsigned char hints[10][4] = {\n" + ",\n".join(
            "  {" + ",".join(str(min(v, 255)) for v in h) + "}" for h in d["hints"]) + "\n};\n")
        f.write("const unsigned int classes[] = {" +
                ",".join(str(s) for s, _ in d["classes"]) + "};\n")

    # the banks' data, as assembly, and the same for the host
    host = open(os.path.join(outdir, "host_data.c"), "w")
    host.write(HEAD + "/* The banks' data for the host build (host.sh). */\n\n")
    for bank, first, end, index, data in banks:
        blob = be16(index) + data
        with open(os.path.join(outdir, f"text_b{bank}_data.asm"), "w") as f:
            f.write(f"; Generated by gen/gen.py -- don't edit.\n; text {first}-{end - 1}: "
                    f"{len(index)} index entries, {len(data)} bytes\n")
            f.write(f"\t.module text_b{bank}_data\n\t.area CODE_{bank}\n"
                    f"\t.globl _text_b{bank}\n_text_b{bank}:\n")
            asm_bytes(f, blob)
        c_bytes(host, f"text_b{bank}", blob)
        with open(os.path.join(outdir, f"text_b{bank}.c"), "w") as f:
            f.write(HEAD + f"/* the printer for text bank {bank}: messages {first}-{end - 1} */\n"
                    f"#ifndef HOST\n#pragma bank {bank}\n#endif\n#include \"advent.h\"\n\n"
                    f"extern const unsigned char text_b{bank}[];\n\n"
                    f"void tsay_b{bank} (unsigned int n) BANKED\n{{\n"
                    f"    const unsigned char *p = text_b{bank} + 2 * (n - TEXT_B{bank}_FIRST);\n"
                    f"    unsigned int o = (p[0] << 8) | p[1];\n\n"
                    f"    if (o == 0xFFFF)\n        return;\n"
                    f"    p = text_b{bank} + 2 * (TEXT_B{bank}_END - TEXT_B{bank}_FIRST) + o;\n"
                    f"    while (*p)\n        outc (*p++);\n}}\n")
    for name, bank, blob in (("travel", TRAVEL_BANK, be16(toffs) + tdata),
                             ("vocab", VOCAB_BANK, vdata)):
        with open(os.path.join(outdir, f"{name}_data.asm"), "w") as f:
            f.write(f"; Generated by gen/gen.py -- don't edit.\n")
            f.write(f"\t.module {name}_data\n\t.area CODE_{bank}\n"
                    f"\t.globl _{name}_data\n_{name}_data:\n")
            asm_bytes(f, blob)
        c_bytes(host, f"{name}_data", blob)
    host.close()

    total = sum(len(t) + 1 for t in msgs if t is not None)
    print(f"text: {len(msgs)} messages, {total} bytes")
    for bank, first, end, index, data in banks:
        print(f"  bank {bank}: {first}-{end - 1}, {2 * len(index) + len(data)} bytes")
    print(f"travel: {2 * len(toffs) + len(tdata)} bytes (bank {TRAVEL_BANK}); "
          f"vocab: {len(vdata)} bytes (bank {VOCAB_BANK})")


if __name__ == "__main__":
    main()
