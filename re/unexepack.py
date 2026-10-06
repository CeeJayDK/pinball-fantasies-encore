import struct,sys
def unpack(path):
    d=open(path,'rb').read()
    h=struct.unpack('<14H',d[:28])
    cparhdr,e_ss,e_sp,e_ip,e_cs=h[4],h[7],h[8],h[10],h[11]
    mod=d[cparhdr*16:]
    hoff=e_cs*16
    hdr=mod[hoff:hoff+e_ip]
    if e_ip==18:
        real_ip,real_cs,mem_start,exepack_size,real_sp,real_ss,dest_len,skip_len,sig=struct.unpack('<8H2s',hdr)
    else:
        real_ip,real_cs,mem_start,exepack_size,real_sp,real_ss,dest_len,sig=struct.unpack('<7H2s',hdr); skip_len=1
    assert sig==b'RB',sig
    print(f'{path}: real cs:ip={real_cs:04x}:{real_ip:04x} ss:sp={real_ss:04x}:{real_sp:04x} dest_len={dest_len:#x} paras ({dest_len*16:#x} bytes) skip_len={skip_len} exepack_size={exepack_size:#x} hdr_off={hoff:#x}')
    src_end=hoff-(skip_len-1)*16
    src=bytearray(mod[:src_end])
    # skip trailing 0xff padding
    si=len(src)
    while si>0 and src[si-1]==0xff: si-=1
    dest=bytearray(dest_len*16)
    di=len(dest)
    while True:
        si-=1; cmd=src[si]
        si-=2; length=struct.unpack('<H',src[si:si+2])[0]
        if cmd&0xfe==0xb0:
            si-=1; fill=src[si]
            di-=length; dest[di:di+length]=bytes([fill])*length
        elif cmd&0xfe==0xb2:
            si-=length; di-=length; dest[di:di+length]=src[si:si+length]
        else:
            raise Exception(f'bad cmd {cmd:#x} at {si:#x}')
        if cmd&1: break
    print(f'  decompressed: src stopped at {si:#x}, dest filled from {di:#x}')
    # relocations
    p=hoff+e_ip+ (exepack_size - e_ip) # placeholder; find stub end: reloc table follows the stub. stub length: search for 'Packed file is corrupt' then locate table
    relocs=[]
    # The reloc table starts right after the stub; stub ends after the error string+'$'? Use exepack_size: table region = mod[hoff+e_ip : hoff+exepack_size]; the stub is fixed 258 bytes for ip=18 variant
    dest[:di]=src[:si]
    q=mod.index(b'Packed file is corrupt',hoff)+len(b'Packed file is corrupt')
    for seg in range(16):
        cnt=struct.unpack('<H',mod[q:q+2])[0]; q+=2
        for i in range(cnt):
            off=struct.unpack('<H',mod[q:q+2])[0]; q+=2
            relocs.append((seg*0x1000+off))
    print(f'  {len(relocs)} relocations; table ended at mod offset {q:#x} (exepack end {hoff+exepack_size:#x})')
    return bytes(dest),relocs,(real_cs,real_ip,real_ss,real_sp)
if __name__=='__main__':
    for name in ['SB16','NOSOUND']:
        img,relocs,ent=unpack(f'/Users/pedro/Dosbox/pinball/{name}.SDR')
        open(f'{name}_unp.bin','wb').write(img)
        open(f'{name}_relocs.txt','w').write('\n'.join(f'{r:05x}: {struct.unpack("<H",img[r:r+2])[0]:04x}' for r in relocs))
        print('  first bytes',img[:24].hex(), ' relocs sample', [f'{r:05x}={struct.unpack("<H",img[r:r+2])[0]:04x}' for r in relocs[:12]])
