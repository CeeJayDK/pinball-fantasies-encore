#!/usr/bin/env python3
"""Prints a stretch of a table's listing, compactly: python3 re/show.py <table> <from> <to> [new]
With "new", for tables 2 to 4: stretches that line up with Party Land's code (re/align.py) are
each said in one line, with where in Party Land, so that only what is the table's own is read."""
import pathlib, re, sys
t, a, b = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
here = pathlib.Path(__file__).parent / 'fantasy'
runs = []
if len(sys.argv) > 4 and t != '1':
    for line in (here / f'map_1_{t}.txt').read_text().splitlines():
        f = line.split()
        if f and f[0] == 'code': runs.append((int(f[3], 16), int(f[3], 16) + int(f[2], 16) - int(f[1], 16), int(f[1], 16)))
def lined_up(at):
    for lo, hi, first in runs:
        if lo <= at <= hi: return first + at - lo
    return None
on, run = False, None
for line in (here / f'TABLE{t}_seg0010.lst').read_text().splitlines():
    m = re.match(r'^([0-9A-F]{4})  ', line)
    if m:
        at = int(m.group(1), 16)
        on = a <= at < b
        if on and runs:
            pl = lined_up(at)
            if pl is not None:
                if run is None: run = [at, at, pl, pl]
                else: run[1], run[3] = at, pl
                continue
            if run:
                print(f'  ... {run[0]:04X}-{run[1]:04X} as Party Land {run[2]:04x}-{run[3]:04x}')
                run = None
    elif not line.strip(): continue
    elif run and on: continue
    if on: print(re.sub(r'\s+;', ' ;', line)[:72])
if run: print(f'  ... {run[0]:04X}-{run[1]:04X} as Party Land {run[2]:04x}-{run[3]:04x}')
