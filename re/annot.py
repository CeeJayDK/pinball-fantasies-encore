"""Turns ndisasm's listing into one that reads: routines and jump targets labelled with who
calls them, and, given a file of names (re/symbols), each named routine and variable noted
beside the instructions that use it.
  python3 re/annot.py <in.asm> <out.lst> [names.txt]"""
import sys, re, collections
src = sys.argv[1]; out = sys.argv[2]
code, data, csvars = {}, {}, {}
if len(sys.argv) > 3:
    for line in open(sys.argv[3]):
        line = line.split('#')[0]
        m = re.match(r'^([cdv]) ([0-9a-f]+) (\w+)(?:\s+@\d:[0-9a-f]+)*\s*(?:;\s*(.*))?$', line.strip())
        if m: {'c': code, 'd': data, 'v': csvars}[m.group(1)][int(m.group(2), 16)] = (m.group(3), m.group(4) or '')
lines = []
for line in open(src):
    try: addr = int(line[:8], 16)
    except ValueError: continue
    lines.append((addr, line[10:28].strip(), line[28:].rstrip()))
calls = collections.defaultdict(list); jumps = collections.defaultdict(list); datarefs = collections.defaultdict(list)
callre = re.compile(r'^call (?:word )?0x([0-9a-f]+)$'); jre = re.compile(r'^j\w+ (?:word )?0x([0-9a-f]+)$')
memre = re.compile(r'\[(cs:)?0x([0-9a-f]+)\]')
for addr, hexs, ins in lines:
    m = callre.match(ins)
    if m: calls[int(m.group(1), 16)].append(addr); continue
    m = jre.match(ins)
    if m: jumps[int(m.group(1), 16)].append(addr)
    for m in memre.finditer(ins):
        if not m.group(1): datarefs[int(m.group(2), 16)].append(addr)
# A run of the same instruction over and over is data, or padding: one line says so.
runs = {}
i = 0
while i < len(lines):
    j = i
    while j + 1 < len(lines) and lines[j + 1][2] == lines[i][2] and lines[j + 1][0] not in calls and lines[j + 1][0] not in jumps: j += 1
    if j - i >= 4:
        runs[lines[i][0]] = (j - i + 1, lines[j][0])
        for k in range(i + 1, j + 1): runs[lines[k][0]] = None
    i = j + 1
with open(out, 'w') as o:
    for addr, hexs, ins in lines:
        if ins == 'nop': continue
        if addr in runs:
            if runs[addr]: o.write(f'{addr:04X}  {ins}   ; the same {runs[addr][0]} times, to {runs[addr][1]:04X}\n')
            continue
        pre = ''
        name = code.get(addr)
        if addr in calls or name:
            who = f'  ; called from {", ".join(f"{a:04x}" for a in calls[addr][:12])}{" ..." if len(calls[addr]) > 12 else ""}' if addr in calls else ''
            pre += f'\nFUNC_{addr:04X}{" " + name[0] if name else ""}:{who}\n'
            if name and name[1]: pre += f'  ; {name[1]}\n'
        elif addr in jumps: pre += f'L_{addr:04X}:  ; from {", ".join(f"{a:04x}" for a in jumps[addr][:6])}\n'
        notes = []
        m = callre.match(ins) or jre.match(ins)
        if m and int(m.group(1), 16) in code: notes.append(code[int(m.group(1), 16)][0])
        for m in memre.finditer(ins):
            table = csvars if m.group(1) else data
            if int(m.group(2), 16) in table: notes.append(table[int(m.group(2), 16)][0])
        o.write(pre + f'{addr:04X}  {ins}' + (f'{" " * max(1, 40 - len(ins))}; {", ".join(notes)}' if notes else '') + '\n')
with open(out + '.xref', 'w') as o:
    for a in sorted(datarefs):
        o.write(f'{a:04x}{" " + data[a][0] if a in data else ""}: {len(datarefs[a])} refs: {" ".join(f"{x:04x}" for x in datarefs[a][:40])}\n')
print('functions:', len(calls), 'data addrs:', len(datarefs))
