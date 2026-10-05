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
        elif '[' in ins[:m.start()] and ']' in ins[m.end():] and ins.rfind('[', 0, m.start()) > ins.rfind(']', 0, m.start()):
            kinds.append('csdata' if ins[ins.rfind('[', 0, m.start()):m.start()].startswith('[cs:') else 'data')
        else: kinds.append('imm')
    return kinds

def main(a, b):
    A, B = load(a), load(b)
    na = [num.sub('#', i) for _, i in A]
    nb = [num.sub('#', i) for _, i in B]
    sm = difflib.SequenceMatcher(None, na, nb, autojunk=False)
    votes = {'code': collections.defaultdict(collections.Counter), 'data': collections.defaultdict(collections.Counter),
             'csdata': collections.defaultdict(collections.Counter),
             'imm': collections.defaultdict(collections.Counter)}
    addr = {}
    small = []
    follows = {A[i][0]: (A[i + 1][0],) for i in range(len(A) - 1)}
    matched = 0
    for i, j, n in sm.get_matching_blocks():
        if n < 6: continue   # short runs match by chance
        matched += n
        for k in range(n):
            (aa, ia), (ab, ib) = A[i + k], B[j + k]
            addr[aa] = ab
            # small numbers that differ between two matched instructions are not addresses:
            # the same routine doing something else in this table (another light, another count)
            for kd, x, y in zip(kind(ia), num.findall(ia), num.findall(ib)):
                if kd == 'imm' and x != y and int(x, 16) < 0x100: small.append((aa, ab, ia, ib))
            for kd, x, y in zip(kind(ia), num.findall(ia), num.findall(ib)):
                votes[kd][int(x, 16)][int(y, 16)] += 1
    with open(here / f'map_{a}_{b}.txt', 'w') as o:
        o.write(f'# TABLE{a} -> TABLE{b}: {matched} of {len(A)} instructions lined up\n')
        o.write('# code: a run of instructions of A (first, last), and where the first is in B\n')
        # as runs: first and last instruction of A, and where the first is in B
        run = None
        for aa in sorted(addr):
            d = addr[aa] - aa
            if run and run[2] == d and aa in follows.get(run[1], ()): run[1] = aa
            else:
                if run: o.write(f'code {run[0]:04x} {run[1]:04x} {run[0] + run[2]:04x}\n')
                run = [aa, aa, d]
        if run: o.write(f'code {run[0]:04x} {run[1]:04x} {run[0] + run[2]:04x}\n')
        o.write('# target: where a routine A calls or jumps to is in B, as B\'s matching instruction has it\n')
        for x in sorted(votes['code']):
            c = votes['code'][x].most_common()
            o.write(f'target {x:04x} {c[0][0]:04x} {c[0][1]}\n')
        o.write('# small: a matched instruction with another small number in B (where in A, where in B, both)\n')
        for aa, ab, ia, ib in small: o.write(f'small {aa:04x} {ab:04x} | {ia} | {ib}\n')
        for kd in ('data', 'csdata', 'imm'):
            o.write(f'# {kd}: number in A, number in B, how often (other candidates)\n')
            for x in sorted(votes[kd]):
                c = votes[kd][x].most_common()
                if kd == 'imm' and x < 0x100: continue
                rest = ' '.join(f'{y:x}x{n}' for y, n in c[1:4])
                o.write(f'{kd} {x:04x} {c[0][0]:04x} {c[0][1]}' + (f'  ({rest})' if rest else '') + '\n')
    print(f'TABLE{a} -> TABLE{b}: {matched} of {len(A)} instructions lined up; {len(votes["data"])} data addresses')

main(int(sys.argv[1]), int(sys.argv[2]))
