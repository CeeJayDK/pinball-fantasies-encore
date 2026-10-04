#!/usr/bin/env python3
"""Lines up one table program's code with another's. The four tables were built from one
engine plus their own rules, so most routines are the same instructions with other addresses.
Matching the instruction streams with the numbers taken out, then pairing the numbers of the
matched instructions, gives where each routine and each variable of table A is in table B.

  python3 re/align.py A B      (table numbers; reads re/fantasy, writes re/fantasy/map_A_B.txt)
"""
import collections, difflib, pathlib, re, sys

here = pathlib.Path(__file__).parent / 'fantasy'
num = re.compile(r'0x[0-9a-f]+')

def load(t):
    f = next(here.glob(f'TABLE{t}_seg0010.asm'))
    out = []
    for line in f.read_text().splitlines():
        try: addr = int(line[:8], 16)
        except ValueError: continue
        ins = line[28:].strip()
        if not ins or line[10:28].strip() == '' or ins.startswith('-'): continue
        out.append((addr, ins))
    return out

def kind(ins):
    """'code' for a jump or call target, 'data' for a number inside brackets, else 'imm'."""
    kinds = []
    for m in num.finditer(ins):
        if re.match(r'^(j\w+|call|loop\w*)\b', ins) and '[' not in ins: kinds.append('code')
        elif '[' in ins[:m.start()] and ']' in ins[m.end():] and ins.rfind('[', 0, m.start()) > ins.rfind(']', 0, m.start()): kinds.append('data')
        else: kinds.append('imm')
    return kinds

def main(a, b):
    A, B = load(a), load(b)
    na = [num.sub('#', i) for _, i in A]
    nb = [num.sub('#', i) for _, i in B]
    sm = difflib.SequenceMatcher(None, na, nb, autojunk=False)
    votes = {'code': collections.defaultdict(collections.Counter), 'data': collections.defaultdict(collections.Counter),
             'imm': collections.defaultdict(collections.Counter)}
    addr = {}
    matched = 0
    for i, j, n in sm.get_matching_blocks():
        if n < 6: continue   # short runs match by chance
        matched += n
        for k in range(n):
            (aa, ia), (ab, ib) = A[i + k], B[j + k]
            addr[aa] = ab
            for kd, x, y in zip(kind(ia), num.findall(ia), num.findall(ib)):
                votes[kd][int(x, 16)][int(y, 16)] += 1
    with open(here / f'map_{a}_{b}.txt', 'w') as o:
        o.write(f'# TABLE{a} -> TABLE{b}: {matched} of {len(A)} instructions lined up\n')
        o.write('# code: where an instruction of A is in B\n')
        prev = None
        for aa in sorted(addr):
            d = addr[aa] - aa
            if d != prev: o.write(f'code {aa:04x} {addr[aa]:04x}\n')   # only where the distance changes
            prev = d
        for kd in ('data', 'imm'):
            o.write(f'# {kd}: number in A, number in B, how often (other candidates)\n')
            for x in sorted(votes[kd]):
                c = votes[kd][x].most_common()
                if kd == 'imm' and (x < 0x100 or (len(c) == 1 and c[0][0] == x)): continue
                rest = ' '.join(f'{y:x}x{n}' for y, n in c[1:4])
                o.write(f'{kd} {x:04x} {c[0][0]:04x} {c[0][1]}' + (f'  ({rest})' if rest else '') + '\n')
    print(f'TABLE{a} -> TABLE{b}: {matched} of {len(A)} instructions lined up; {len(votes["data"])} data addresses')

main(int(sys.argv[1]), int(sys.argv[2]))
