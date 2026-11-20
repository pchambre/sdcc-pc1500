#!/usr/bin/env python3
"""fuzz.py - random commands for host.sh: one or two words from the
game's vocabulary (or a word it doesn't know), with yes/no answers mixed
in, so the port and jwodder's original can be compared on many turns.
SUSPEND/RESUME are left out (the port keeps the game in the card's RAM,
the original in a file).

Usage: fuzz.py orig/vocab.c count seed > commands
"""

import random
import re
import sys

words = re.findall(r'\{"([^"\\]*(?:\\.[^"\\]*)*)", (\d+), (\d+)\}', open(sys.argv[1]).read())
skip = {2030, 2032}     # SUSPEND, RESUME and their synonyms
vocab = [w.replace('\\"', '"') for w, v1, _ in words if int(v1) not in skip]
motions = [w for w, v1, _ in words if int(v1) < 1000]
r = random.Random(int(sys.argv[3]))
for _ in range(int(sys.argv[2])):
    x = r.random()
    if x < 0.45:
        print(r.choice(motions))
    elif x < 0.75:
        print(r.choice(vocab), r.choice(vocab))
    elif x < 0.9:
        print(r.choice(vocab))
    elif x < 0.97:
        print(r.choice(["yes", "no", "y", "n"]))
    else:
        print(r.choice(["xyzzy", "plugh", "frob", "", "  ", "get lamp now"]))
