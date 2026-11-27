"""No game data: synthetic NUP/NSO fixtures with nested codecs and hashes."""
import hashlib,pathlib,struct,subprocess,sys,tempfile,unittest
EXE=pathlib.Path(sys.argv.pop(1)).resolve()
def literal(b):
    if len(b)<15:return bytes([len(b)<<4])+b
    n=len(b)-15;ext=b'\xff'*(n//255)+bytes([n%255]);return b'\xf0'+ext+b
def nso(compressed=False,overlap=False):
    text=bytearray(256);struct.pack_into('<I',text,4,0x20)
    struct.pack_into('<II',text,0x10,0xd40006c1,0xd40003e1) # SVC 0x36 must remain unchanged.
    text[0x20:0x24]=b'MOD0';struct.pack_into('<6i',text,0x24,0x1020-0x20,0x3000-0x20,0x4000-0x20,0,0,0x3000-0x20)
    segments=[bytes(text),bytes(64),b'DATA'+bytes(12)]
    h=bytearray(256);h[:4]=b'NSO0';struct.pack_into('<I',h,12,0x38|(7 if compressed else 0));struct.pack_into('<I',h,0x3c,0x1000)
    payload=b''
    for i,(mem,b) in enumerate(zip([0,0x1000,0x2000],segments)):
        encoded=literal(b) if compressed else b
        if compressed and overlap and i==1:encoded=b'\x1f\x00\x01\x00\x2c\x00'
        struct.pack_into('<III',h,16+i*16,256+len(payload),mem,len(b));struct.pack_into('<I',h,0x60+4*i,len(encoded));h[0xa0+i*32:0xc0+i*32]=hashlib.sha256(b).digest();payload+=encoded
    image=segments[0]+bytes(0x1000-256)+segments[1]+bytes(0x1000-64)+segments[2]
    return bytes(h)+payload,hashlib.sha256(image).hexdigest()
def zstd_raw(b):
    if len(b)<256:h=b'\x20'+bytes([len(b)])
    else:h=b'\x60'+struct.pack('<H',len(b)-256)
    return b'\x28\xb5\x2f\xfd'+h+((len(b)<<3)|1).to_bytes(3,'little')+b
def nso_zbic(modes=('zbic','zbic','zbic'),table=False):
    original,_=nso();h=bytearray(original[:256]);segments=[];payload=b'';flags=0x38|0x80
    for i,mode in enumerate(modes):
        off,_,size=struct.unpack_from('<III',original,16+i*16);data=original[off:off+size]
        if table and i==2:data=b'A'*100
        segments.append(data)
        if mode=='plain':encoded=data
        elif mode=='lz4':encoded=literal(data);flags|=1<<i
        else:
            encoded=bytes.fromhex('5a42494320646500000841019403607203002a6004') if table and i==2 else b'ZBIC'+zstd_raw(data)[4:]
            flags|=1<<i
        struct.pack_into('<I',h,16+i*16,256+len(payload));struct.pack_into('<I',h,24+i*16,len(data));struct.pack_into('<I',h,0x60+4*i,len(encoded));h[0xa0+32*i:0xc0+32*i]=hashlib.sha256(data).digest();payload+=encoded
    struct.pack_into('<I',h,12,flags)
    image=segments[0]+bytes(0x1000-len(segments[0]))+segments[1]+bytes(0x1000-len(segments[1]))+segments[2]
    return bytes(h)+payload,hashlib.sha256(image).hexdigest()

def nup(packages,codec=0,corrupt_file_hash=False,truncate=False,cycle=False):
    # packages: (kind, version, {module name: NSO bytes}); one root and one ExeFS per package.
    pool=bytearray();names={}
    def name(s):
        b=s.encode();at=len(pool);pool.extend(b);return at,len(b)
    dirs=[(0,0,0,len(packages))];children=list(range(1,1+len(packages)));files=[]
    for i,(_,_,mods) in enumerate(packages):
        first=len(children)
        for filename,data in mods.items():
            children.append(1+len(packages)+len(files));files.append((name(filename),data))
        dirs.append((*name('exefs'+str(i)),first,len(mods)))
    if cycle:children[-1]=1
    table_size=0x30+len(packages)*0x40+len(dirs)*0x18+len(files)*0x50
    pool_at=table_size+len(children)*4
    toc=bytearray(pool_at+len(pool));toc[:4]=b'TOC\1';struct.pack_into('<IQQQQ',toc,4,len(packages),len(dirs),len(files),pool_at,len(pool))
    for i,(kind,version,_) in enumerate(packages):struct.pack_into('<QIIIIBB',toc,0x30+i*0x40,0x0400000000000000,0,i+1,0xffffffff,version,kind,0)
    at=0x30+len(packages)*0x40
    for d in dirs:struct.pack_into('<IIII',toc,at,*d);at+=0x18
    blob=0x80+len(toc);payload=b''
    for (no,nl),data in files:
        stored=zstd_raw(data) if codec==1 else data
        if truncate:stored=stored[:-1]
        struct.pack_into('<IIQQQ',toc,at,no,nl,len(data),blob+len(payload),len(stored));toc[at+32:at+64]=bytes(32) if corrupt_file_hash else hashlib.sha256(data).digest();toc[at+64]=codec;at+=0x50;payload+=stored
    for i,node in enumerate(children):struct.pack_into('<I',toc,table_size+4*i,node)
    toc[pool_at:]=pool;h=bytearray(128);h[:4]=b'NUP\1';struct.pack_into('<QQ',h,8,128,len(toc));h[24:56]=hashlib.sha256(toc).digest();struct.pack_into('<QQI',h,56,blob,blob+len(payload),len(packages));return bytes(h+toc)+payload

class Program(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name)
    def tearDown(self):self.temp.cleanup()
    def run_program(self,b,ok=True,split=False):
        p=self.root/'test.nup'
        if split:
            p.mkdir()
            for i,at in enumerate(range(0,len(b),137)):(p/f'{i:03}').write_bytes(b[at:at+137])
        else:p.write_bytes(b)
        result=subprocess.run([str(EXE),str(p),'--prepare'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0 if ok else 1,result.stdout+result.stderr);return result.stdout+result.stderr
    def test_plain_and_svc_inventory(self):
        raw,sha=nso();out=self.run_program(nup([(0,0,{'main':raw})]));self.assertIn(sha,out);self.assertIn('svcs=2',out)
    def test_nested_zstd_lz4_split(self):
        raw,sha=nso(True);self.assertIn(sha,self.run_program(nup([(0,0,{'main':raw})],codec=1),split=True))
    def test_lz4_overlapping_match(self):
        raw,sha=nso(True,True);self.assertIn(sha,self.run_program(nup([(0,0,{'main':raw})])))
    def test_latest_update_ignores_unusable_base(self):
        raw,sha=nso();out=self.run_program(nup([(0,0,{'main':b'bad base'}),(1,65536,{'main':raw})]));self.assertIn('version=65536',out);self.assertIn(sha,out)
    def test_update_wins_tied_version_independent_of_package_order(self):
        raw,sha=nso();out=self.run_program(nup([(1,0,{"main":raw}),(0,0,{"main":b"bad base"})]));self.assertIn(sha,out)
    def test_update_with_title_id_800_variant(self):
        raw,sha=nso();blob=bytearray(nup([(0,0,{"main":b"invalid base"}),(1,1,{"main":raw})]))
        struct.pack_into('<Q',blob,0x80+0x30+0x40,0x0400000000000800)
        size=struct.unpack_from('<Q',blob,16)[0];blob[24:56]=hashlib.sha256(blob[128:128+size]).digest()
        self.assertIn(sha,self.run_program(bytes(blob)))
    def test_standalone_update_package(self):
        raw,sha=nso();self.assertIn(sha,self.run_program(nup([(1,327680,{'main':raw})])))
    def test_standalone_update_package_split(self):
        raw,sha=nso(True);self.assertIn(sha,self.run_program(nup([(1,327680,{'main':raw})],codec=1),split=True))
    def test_standalone_updates_choose_latest_program(self):
        raw,sha=nso();self.assertIn(sha,self.run_program(nup([(1,1,{'main':b'old invalid image'}),(1,2,{'main':raw})])))
    def test_standalone_update_without_program_reports_missing_main(self):
        raw,_=nso();self.assertIn('No main module',self.run_program(nup([(1,1,{'sdk':raw})]),ok=False))
    def test_only_dlc_is_not_an_application(self):
        raw,_=nso();self.assertIn('No application or update',self.run_program(nup([(2,1,{'main':raw})]),ok=False))
    def test_standalone_updates_from_different_titles_rejected(self):
        raw,_=nso();blob=bytearray(nup([(1,1,{'main':raw}),(1,2,{'main':raw})]))
        struct.pack_into('<Q',blob,0x80+0x30+0x40,0x0400000000001000)
        size=struct.unpack_from('<Q',blob,16)[0];blob[24:56]=hashlib.sha256(blob[128:128+size]).digest()
        self.assertIn('different titles',self.run_program(bytes(blob),ok=False))
    def test_skip_dlc(self):
        raw,_=nso();self.run_program(nup([(0,0,{'main':raw}),(2,999999,{'main':b'bad dlc'})]))
    def test_nup_payload_hash(self):
        raw,_=nso();self.assertIn('Program file SHA-256',self.run_program(nup([(0,0,{'main':raw})],corrupt_file_hash=True),ok=False))
    def test_nso_segment_hash(self):
        raw,_=nso();raw=bytearray(raw);raw[-1]^=1;self.assertIn('NSO segment SHA-256',self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False))
    def test_truncated_zstd(self):
        raw,_=nso();self.run_program(nup([(0,0,{'main':raw})],codec=1,truncate=True),ok=False)
    def test_unsupported_nup_codec(self):
        raw,_=nso();self.assertIn('codec',self.run_program(nup([(0,0,{'main':raw})],codec=2),ok=False))
    def test_directory_cycle(self):
        raw,_=nso();self.assertIn('cyclic',self.run_program(nup([(0,0,{'main':raw})],cycle=True),ok=False))
    def test_invalid_lz4_offset(self):
        raw,_=nso(True,True);raw=bytearray(raw);ro=struct.unpack_from('<I',raw,0x20)[0];raw[ro+2:ro+4]=b'\0\0';self.assertIn('LZ4 match offset',self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False))
    def test_excessive_nso_segment(self):
        raw,_=nso();raw=bytearray(raw);struct.pack_into('<I',raw,0x24,0xffffff00);self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False)
    def test_unknown_subsdk_not_silently_skipped(self):
        raw,_=nso();self.assertIn("additional ExeFS module",self.run_program(nup([(0,0,{"main":raw,"subsdk10":raw})]),ok=False))
    def test_zbic_plain_blocks(self):
        raw,sha=nso_zbic();self.assertIn(sha,self.run_program(nup([(0,0,{'main':raw})])))
    def test_zbic_probability_table_and_outer_zstd_split(self):
        raw,sha=nso_zbic(table=True);self.assertIn(sha,self.run_program(nup([(0,0,{'main':raw})],codec=1),split=True))
    def test_mixed_zbic_lz4_uncompressed_segments(self):
        raw,sha=nso_zbic(('zbic','lz4','plain'));self.assertIn(sha,self.run_program(nup([(0,0,{'main':raw})])))
    def test_zbic_segment_hash_still_checked(self):
        raw,_=nso_zbic();raw=bytearray(raw);raw[0xa0]^=1;self.assertIn('NSO segment SHA-256',self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False))
    def test_zbic_declared_size_must_match(self):
        raw,_=nso_zbic();raw=bytearray(raw);struct.pack_into('<I',raw,0x18,257);self.assertIn('ZBIC',self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False))
    def test_zbic_reported_explicitly(self):
        raw,_=nso(True);raw=bytearray(raw);raw[12]|=0x80;start=struct.unpack_from('<I',raw,0x10)[0];raw[start:start+4]=b'ZBIC';self.assertIn('ZBIC',self.run_program(nup([(0,0,{'main':bytes(raw)})]),ok=False))
if __name__=='__main__':unittest.main()
