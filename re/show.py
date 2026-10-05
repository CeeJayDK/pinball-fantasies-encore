#!/usr/bin/env python3
"""Prints a stretch of a table's listing, compactly: python3 re/show.py <table> <from> <to>"""
import pathlib, re, sys
t, a, b = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
on = False
for line in (pathlib.Path(__file__).parent / 'fantasy' / f'TABLE{t}_seg0010.lst').read_text().splitlines():
    m = re.match(r'^([0-9A-F]{4})  ', line)
    if m:
        at = int(m.group(1), 16)
        on = a <= at < b
    elif not line.strip(): continue
    if on: print(re.sub(r'\s+;', ' ;', line)[:72])
