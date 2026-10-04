import sys, re, collections
src=sys.argv[1]; out=sys.argv[2]
lines=[]
for line in open(src):
    try: addr=int(line[:8],16)
    except: continue
    lines.append((addr,line[10:28].strip(),line[28:].rstrip()))
calls=collections.defaultdict(list); jumps=collections.defaultdict(list); datarefs=collections.defaultdict(list)
callre=re.compile(r'^call (?:word )?0x([0-9a-f]+)$'); jre=re.compile(r'^j\w+ (?:word )?0x([0-9a-f]+)$')
memre=re.compile(r'\[(?:cs:)?0x([0-9a-f]+)\]')
for addr,hexs,ins in lines:
    m=callre.match(ins)
    if m: calls[int(m.group(1),16)].append(addr); continue
    m=jre.match(ins)
    if m: jumps[int(m.group(1),16)].append(addr)
    for m in memre.finditer(ins): datarefs[int(m.group(1),16)].append(addr)
with open(out,'w') as o:
    for addr,hexs,ins in lines:
        if ins=='nop': continue
        pre=''
        if addr in calls: pre+=f'\nFUNC_{addr:04X}:  ; called from {", ".join(f"{a:04x}" for a in calls[addr][:12])}{" ..." if len(calls[addr])>12 else ""}\n'
        elif addr in jumps: pre+=f'L_{addr:04X}:  ; from {", ".join(f"{a:04x}" for a in jumps[addr][:6])}\n'
        o.write(pre+f'{addr:04X}  {ins}\n')
with open(out+'.xref','w') as o:
    for a in sorted(datarefs):
        o.write(f'{a:04x}: {len(datarefs[a])} refs: {" ".join(f"{x:04x}" for x in datarefs[a][:40])}\n')
print('functions:',len(calls),'data addrs:',len(datarefs))
