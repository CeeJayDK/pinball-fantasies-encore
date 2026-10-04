#!/usr/bin/env python3
"""Disassembles the game's programs into re/fantasy: per program, the segment map, a raw and an
annotated listing of each code segment, and a hex dump of every other segment.
  python3 re/disasm.py <the game's folder>"""
import re, struct, subprocess, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent))

def mz(path):
    d = path.read_bytes()
    assert d[:2] == b'MZ', path
    h = struct.unpack('<14H', d[:28])
    last, pages, nreloc, hdrpar = h[1], h[2], h[3], h[4]
    ip, cs, reloff = h[10], h[11], h[12]
    size = pages * 512 - ((512 - last) % 512 if last else 0)
    image = d[hdrpar * 16:size]
    segs, code = {cs}, {cs}
    for i in range(nreloc):
        off, seg = struct.unpack_from('<HH', d, reloff + 4 * i)
        at = seg * 16 + off
        value = struct.unpack_from('<H', image, at)[0]
        segs.add(value)
        # A far call or jump's segment word names a code segment.
        if at >= 3 and image[at - 3] in (0x9a, 0xea): code.add(value)
    segs = sorted(s for s in segs if s * 16 < len(image))
    return image, hdrpar * 16, cs, ip, segs, code

target = re.compile(r'^\S+\s+\S+\s+(?:call|jmp|j\w+|loop\w*|jcxz)(?: near| short| word)? 0x([0-9a-f]+)$')

def disassemble(binf):
    """ndisasm reads straight through, and loses its place wherever data sits between
    routines. Every address the code jumps to or calls is certainly an instruction, so those
    are given back to it as places to start afresh, until no new ones turn up."""
    size = binf.stat().st_size
    sync, text = set(), ''
    for _ in range(6):
        args = ['ndisasm', '-b16'] + [a for t in sorted(sync) for a in ('-s', hex(t))] + [str(binf)]
        text = subprocess.run(args, capture_output=True, text=True).stdout
        found = {int(m.group(1), 16) for line in text.splitlines() if (m := target.match(line))}
        found = {t for t in found if t < size}
        if found <= sync: break
        sync |= found
    return text

def hexdump(data):
    out = []
    for i in range(0, len(data), 16):
        row = data[i:i + 16]
        out.append(f'{i:04x}  {row.hex(" "):<47}  ' + ''.join(chr(b) if 32 <= b < 127 else '.' for b in row))
    return '\n'.join(out) + '\n'

def main(game, out):
    out.mkdir(parents=True, exist_ok=True)
    for path in sorted(game.glob('*.PRG')) + sorted(game.glob('*.EXE')) + sorted(game.glob('*.SDR')):
        if path.read_bytes()[:2] != b'MZ':
            (out / f'{path.stem}_{path.suffix[1:]}.bin').write_bytes(path.read_bytes())
            raw = out / f'{path.stem}_{path.suffix[1:]}.asm'
            raw.write_text(subprocess.run(['ndisasm', '-b16', str(path)], capture_output=True, text=True).stdout)
            continue
        if path.suffix == '.SDR':
            # The sound drivers are EXEPACKed: unpack, then list the whole image as one segment.
            import unexepack, io, contextlib
            with contextlib.redirect_stdout(io.StringIO()):
                image, relocs, (rcs, rip, _, _) = unexepack.unpack(str(path))
            name = f'{path.stem}_SDR'
            binf = out / f'{name}.bin'; binf.write_bytes(image)
            asm = out / f'{name}.asm'
            asm.write_text(disassemble(binf))
            subprocess.run([sys.executable, str(pathlib.Path(__file__).parent / 'annot.py'), str(asm), str(out / f'{name}.lst')], capture_output=True)
            (out / f'{name}_segments.txt').write_text(f'{path.name}: unpacked {len(image)} bytes; entry cs:ip={rcs:04x}:{rip:04x}; {len(relocs)} relocations\n')
            print(path.name, 'unpacked', len(image), f'entry {rcs:04x}:{rip:04x}')
            continue
        image, hdr, cs, ip, segs, code = mz(path)
        name = path.stem if path.suffix == '.PRG' else f'{path.stem}_{path.suffix[1:]}'
        lines = [f'{path.name}: MZ header {hdr} bytes; entry cs:ip={cs:04x}:{ip:04x}; file offset = {hdr} + seg*16']
        for n, seg in enumerate(segs):
            end = segs[n + 1] * 16 if n + 1 < len(segs) else len(image)
            data = image[seg * 16:end]
            # The tables' second code segment (the same 5248 bytes in each) is reached through
            # far pointers, which the relocations do not tell apart from data.
            kind = 'code' if seg in code or (name.startswith('TABLE') and len(data) == 5248) else 'data'
            lines.append(f'  seg {seg:04x}: file offset {hdr + seg * 16:7} ({hdr + seg * 16:#x}) length {len(data):6} {kind}')
            if kind == 'code':
                binf = out / f'{name}_seg{seg:04x}.bin'
                binf.write_bytes(data)
                asm = out / f'{name}_seg{seg:04x}.asm'
                asm.write_text(disassemble(binf))
                subprocess.run([sys.executable, str(pathlib.Path(__file__).parent / 'annot.py'), str(asm),
                                str(out / f'{name}_seg{seg:04x}.lst')], capture_output=True)
            elif data[:4] != b'FORM':
                (out / f'{name}_seg{seg:04x}.hex').write_text(hexdump(data))
        (out / f'{name}_segments.txt').write_text('\n'.join(lines) + '\n')
        print(lines[0], f'{len(segs)} segments, code: {" ".join(f"{s:04x}" for s in sorted(code))}')

main(pathlib.Path(sys.argv[1]), pathlib.Path(__file__).parent / 'fantasy')
