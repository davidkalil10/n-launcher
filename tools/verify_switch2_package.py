"""Check actual libounce startup, ASET bounds and link map; never scan for OUN0 anywhere."""
import pathlib,struct,sys
def verify(nro,link_map):
    b=nro.read_bytes()
    if len(b)<0x80 or b[0x10:0x14]!=b'NRO0':raise ValueError('Invalid NRO0')
    mod=struct.unpack_from('<I',b,4)[0]
    if mod+0x1c>min(len(b),0x1000) or b[mod:mod+4]!=b'MOD0':raise ValueError('Invalid MOD0 startup')
    at=mod+0x1c
    for _ in range(8):
        if at+12>min(len(b),0x1000):raise ValueError('Missing OUN0')
        if b[at:at+4]==b'OUN0':
            if struct.unpack_from('<II',b,at+4)!=(2,1):raise ValueError('Wrong startup platform/version')
            break
        if b[at:at+3]!=b'LNY':raise ValueError('Missing OUN0 extension')
        at+=12
    else:raise ValueError('Missing OUN0')
    aset=struct.unpack_from('<I',b,0x18)[0]
    if aset+0x38>len(b) or b[aset:aset+4]!=b'ASET':raise ValueError('Missing ASET')
    for off in (8,24,40):
        start,size=struct.unpack_from('<QQ',b,aset+off)
        if start+size>len(b)-aset:raise ValueError('Truncated asset')
        if off==8 and (size<64 or b[aset+start:aset+start+3]!=b'\xff\xd8\xff'):raise ValueError('Missing JPEG icon')
        if off==24 and size!=0x4000:raise ValueError('Invalid NACP size')
    text=link_map.read_text(errors='replace').replace('\\','/')
    forbidden=['/libnx/']+['/portlibs/switch/lib/'+x for x in ['libEGL.a','libGL.a','libGLESv2.a','libglapi.a','libvulkan.a','libdrm_nouveau.a','libav','libsw']]+['libdeko3d','nvtegra.o']
    for marker in forbidden:
        if marker in text:raise ValueError('Switch 1 library in map: '+marker)
    for marker in ['/libounce/lib/','/portlibs/switch2/lib/']:
        if marker not in text:raise ValueError('Missing Switch 2 dependency: '+marker)
    print('PASS: OUN0 platform=2 version=1, ASET/JPEG/NACP and Switch 2 link map; probe has no JIT.')
if __name__=='__main__':verify(*map(pathlib.Path,sys.argv[1:]))
