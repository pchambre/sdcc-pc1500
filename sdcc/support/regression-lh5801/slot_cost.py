"""slot_cost.py <dir> <name>: what a profiled build's stack slots (gen.c's
@slot comments, LH5801_MARK) and saved frames (__lh5801_fsave/frest) cost,
in bytes and in cycles -- see profile.sh. Paul Chambre, 2026."""
import re, sys, os
d, n = sys.argv[1], sys.argv[2]
lst = open(os.path.join(d, n + '.lst')).read().splitlines()
mp = open(os.path.join(d, n + '-0100-pc1500.map')).read()
prof = {}
for line in open(os.path.join(d, n + '.prof')):
    a, h, c = line.split(); prof[int(a, 16)] = (int(h), int(c))
syms = {m.group(2): int(m.group(1), 16) for m in re.finditer(r'^\s+([0-9A-F]{8})\s+(\S+)', mp, re.M)}
# the module's CODE base: a label in its listing vs the map
area, inslot, base = None, False, None
rows = []  # (offset, nbytes, region, func)
func = None
for l in lst:
    m = re.match(r'^\s+([0-9A-F]{4})((?: [0-9A-F]{2})+)\s+\d+\s+(.*)$', l)
    text = l[32:] if len(l) > 32 else ''
    if '.area' in text:
        area = text.split()[1]
    if '@slot+' in text: inslot = True
    if '@slot-' in text: inslot = False
    lm = re.match(r'^\s+([0-9A-F]{4})\s+\d+\s+(_\w+):', l)
    if lm and area == 'CODE':
        func = lm.group(2)
        if base is None and func in syms: base = syms[func] - int(lm.group(1), 16)
    if m and area == 'CODE':
        off = int(m.group(1), 16); nb = len(m.group(2).split()); ins = m.group(3)
        if 'sjp' in ins and ('__lh5801_fsave' in ins or '__lh5801_frest' in ins):
            reg = 'call'
        elif inslot: reg = 'slot'
        else: reg = 'body'
        rows.append((off, nb, reg, func))
assert base is not None
# fsave/frest bodies: from their symbols to the next symbol
addrs = sorted(set(syms.values()))
def span(s):
    a = syms[s]; nxt = min([x for x in addrs if x > a] or [a + 64]); return a, nxt
helpers = {s: span(s) for s in ('__lh5801_fsave', '__lh5801_frest')}
# the call sites' inline operands: 4 data bytes after each sjp
size = {'slot': 0, 'call': 0, 'body': 0}; cyc = {'slot': 0, 'call': 0, 'body': 0}
perf = {}
for off, nb, reg, f in rows:
    size[reg] += nb + (4 if reg == 'call' else 0)
    h, c = prof.get(base + off, (0, 0))
    cyc[reg] += c
    pf = perf.setdefault(f, {'slot': 0, 'call': 0, 'body': 0, 'sb': 0, 'cb': 0, 'bb': 0})
    pf[reg] += c; pf[reg[0] + 'b'] += nb + (4 if reg == 'call' else 0)
hc = {s: sum(prof.get(a, (0, 0))[1] for a in range(*r)) for s, r in helpers.items()}
hb = {s: r[1] - r[0] for s, r in helpers.items()}
total = sum(c for _, c in prof.values())
mod = sum(size.values())
print(f'{n}: module code {mod} B, run {total} cycles in all')
print(f'  slot copies   {size["slot"]:5} B  {cyc["slot"]:8} cyc')
print(f'  fsave/frest call sites {size["call"]:2} B  {cyc["call"]:8} cyc (the sjp)')
for s in helpers: print(f'  {s:16} {hb[s]:3} B (library)  {hc[s]:8} cyc')
print(f'  rest of module {size["body"]:5} B  {cyc["body"]:8} cyc')
print('  per function (cycles: body / slot / call; bytes: body / slot / call):')
for f, p in perf.items():
    if p['slot'] or p['call'] or p['sb'] or p['cb']:
        print(f'    {f:28} {p["body"]:7} / {p["slot"]:6} / {p["call"]:5}   {p["bb"]:4} / {p["sb"]:4} / {p["cb"]:3}')
