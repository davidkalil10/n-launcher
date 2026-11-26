"""Validate the synthetic image and embed it; input is objcopy's raw image."""
import pathlib,struct,sys
def verify(b):
    if len(b)!=0x4000 or b[0x10:0x14]!=b'NRO0':raise ValueError('Bad fixture size/magic')
    if struct.unpack_from('<11I',b,0x14)!=(0,0x4000,1,0,0x2000,0x2000,0x1000,0x3000,0x1000,0x1000,0):raise ValueError('Bad fixture layout')
    if struct.unpack_from('<I',b,0x2000)[0]!=0x1357 or struct.unpack_from('<I',b,0x3000)[0]!=0x2468:raise ValueError('Bad fixture constants')
    # ADR/LDR/ADD/STR/EOR/RET only; no privileged call or unresolved linker fixup.
    words=struct.unpack_from('<14I',b,0x1000)
    if words[-1]!=0xd65f03c0:raise ValueError('Missing RET')
    for w in words:
        if w&0xffe0001f==0xd4000001:raise ValueError('Unexpected SVC')
    for index,target in ((0,0x2000),(2,0x3000),(6,0x4000)):
        w=words[index]
        if w&0x9f00001f!=0x10000003:raise ValueError('Expected ADR x3')
        off=(((w>>5)&0x7ffff)<<2)|((w>>29)&3)
        if off&(1<<20):off-=1<<21
        if 0x1000+index*4+off!=target:raise ValueError('Unresolved/wrong ADR target')
    return words
if __name__=='__main__':
    source,destination=map(pathlib.Path,sys.argv[1:]);b=source.read_bytes();verify(b)
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text('#pragma once\ninline constexpr unsigned char fixture_image[]={\n'+',\n'.join(','.join(str(v) for v in b[i:i+32]) for i in range(0,len(b),32))+'\n};\n')
    print('PASS: synthetic image layout, fixed PC-relative references and return; no SVC')
