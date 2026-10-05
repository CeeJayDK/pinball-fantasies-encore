#!/usr/bin/env python3
"""Writes the two files the engine knows the four table programs by.

src/engine/table/Names.inc: the names in re/symbols/table1.txt, as Party Land's addresses.
The engine's code is written against Party Land's addresses throughout.

src/engine/table/Maps.inc: where each of Party Land's addresses is in the other three
programs, from lining them up (re/align.py, run first for 1 2, 1 3 and 1 4): every variable
its engine code touches, every address-like number that code carries, and every run of
instructions that is the same in both. A line of the names file may place a name by hand,
with `@2:1234` after it, where the line-up could not.

  python3 re/gensymbols.py"""
import pathlib, re

here = pathlib.Path(__file__).parent
dest = here.parent / 'src' / 'engine' / 'table'
dest.mkdir(parents=True, exist_ok=True)

tables = {}
for t in (2, 3, 4):
    m = {'code': [], 'data': {}, 'imm': {}, 'csdata': {}, 'target': {}}
    for line in (here / 'fantasy' / f'map_1_{t}.txt').read_text().splitlines():
        f = line.split()
        if not f or f[0].startswith('#'): continue
        if f[0] == 'code': m['code'].append((int(f[1], 16), int(f[2], 16), int(f[3], 16)))
        elif int(f[1], 16) <= 0xffff and int(f[2], 16) <= 0xffff: m[f[0]][int(f[1], 16)] = int(f[2], 16)
    tables[t] = m

names = ['// Written by re/gensymbols.py from re/symbols/table1.txt. Do not edit.',
         '// Party Land\'s addresses: c code, d data, v variables kept among the code, i values.', '']
kinds = {'c': 'code', 'd': 'data', 'v': 'csdata', 'i': 'imm'}
for line in (here / 'symbols' / 'table1.txt').read_text().splitlines():
    m = re.match(r'^([cdiv]) ([0-9a-f]+) (\w+)((?:\s+@\d:[0-9a-f]+)*)\s*(?:;\s*(.*))?$', line.split('#')[0].strip())
    if not m: continue
    kind, addr, name, hand, note = m.group(1), int(m.group(2), 16), m.group(3), m.group(4), m.group(5)
    names.append(f'constexpr u16 {name} = 0x{addr:04x};' + (f'  ///< {note}' if note else ''))
    for t, a in re.findall(r'@(\d):([0-9a-f]+)', hand):
        if kind == 'c': tables[int(t)]['code'].append((addr, addr, int(a, 16)))
        else: tables[int(t)][kinds[kind]][addr] = int(a, 16)
(dest / 'Names.inc').write_text('\n'.join(names) + '\n')

out = ['// Written by re/gensymbols.py from the line-up of the four table programs. Do not edit.', '']
for kind, macro in (('data', 'ENCORE_MAP_DATA'), ('csdata', 'ENCORE_MAP_CODE_DATA'), ('imm', 'ENCORE_MAP_VALUE'), ('target', 'ENCORE_MAP_TARGET')):
    keys = sorted(set().union(*(tables[t][kind].keys() for t in (2, 3, 4))))
    for k in keys:
        # 0xffff: not found in that table
        row = [tables[t][kind].get(k, 0xffff) for t in (2, 3, 4)]
        out.append(f'{macro}(0x{k:04x}, ' + ', '.join(f'0x{a:04x}' for a in row) + ')')
for t in (2, 3, 4):
    for a, b, to in sorted(tables[t]['code']):
        out.append(f'ENCORE_MAP_CODE({t - 1}, 0x{a:04x}, 0x{b:04x}, 0x{to:04x})')
(dest / 'Maps.inc').write_text('\n'.join(out) + '\n')
print(f'{len(names) - 3} names; {len(out) - 2} map lines')
