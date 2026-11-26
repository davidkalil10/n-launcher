"""Streaming cache fixtures: synthetic Zstd RLE frames and NUP RomFS only."""
import hashlib,pathlib,struct,subprocess,sys,tempfile,unittest
EXE=pathlib.Path(sys.argv.pop(1)).resolve()
def fixture(data,corrupt=False,trailing=False,raw=False,kind=0):
    if not raw:assert len(set(data))==1
    frame=b'\x28\xb5\x2f\xfd\xa0'+struct.pack('<I',len(data))
    for at in range(0,len(data),128*1024):
        n=min(128*1024,len(data)-at);frame+=((n<<3)|(0 if raw else 2)|int(at+n==len(data))).to_bytes(3,'little')+(data[at:at+n] if raw else data[:1])
    if trailing:frame+=b'bad'
    toc=bytearray(0x30+0x40+0x18+0x50+4+4);toc[:4]=b'TOC\1';pool=len(toc)-4
    struct.pack_into('<IQQQQ',toc,4,1,1,1,pool,4)
    struct.pack_into('<QIIIIBB',toc,0x30,0x0400f00000001000,0,0xffffffff,0,0,kind,0)
    struct.pack_into('<IIII',toc,0x70,0,0,0,1)
    f=0x88;struct.pack_into('<IIQQQ',toc,f,0,4,len(data),0x80+len(toc),len(frame));toc[f+32:f+64]=hashlib.sha256(data+(b'corrupt' if corrupt else b'')).digest();toc[f+64]=1
    struct.pack_into('<I',toc,pool-4,1);toc[pool:]=b'test'
    h=bytearray(128);h[:4]=b'NUP\1';struct.pack_into('<QQ',h,8,128,len(toc));h[24:56]=hashlib.sha256(toc).digest();struct.pack_into('<QQI',h,56,128+len(toc),128+len(toc)+len(frame),1)
    return bytes(h+toc)+frame
class Cache(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name);self.source=self.root/'jogo.nup';self.cache=self.root/'cache'
    def tearDown(self):self.tmp.cleanup()
    def run_cache(self,*extra,ok=True):
        p=subprocess.run([str(EXE),str(self.source),str(self.cache),'0','1048576',*extra],capture_output=True,text=True)
        self.assertEqual(p.returncode,0 if ok else 1,p.stdout+p.stderr);return p.stdout+p.stderr
    def test_streamed_parts_and_reuse(self):
        data=b'K'*(2*1048576+17);self.source.write_bytes(fixture(data));self.run_cache();folder=self.cache/hashlib.sha256(data).hexdigest()
        self.assertEqual(b''.join(p.read_bytes() for p in sorted(folder.glob('*.bin'))),data);self.assertEqual((folder/'complete').stat().st_size,48)
        before=(folder/'0000.bin').stat().st_mtime_ns;self.run_cache();self.assertEqual(before,(folder/'0000.bin').stat().st_mtime_ns)
    def test_split_matches_full(self):
        data=b'P'*200000;blob=fixture(data);self.source.mkdir()
        for i,at in enumerate(range(0,len(blob),137)):(self.source/f'{i:02}').write_bytes(blob[at:at+137])
        self.run_cache();self.assertEqual((self.cache/hashlib.sha256(data).hexdigest()/'0000.bin').read_bytes(),data)
    def test_standalone_update_romfs_split(self):
        data=b'U'*200000;blob=fixture(data,kind=1);self.source.mkdir()
        for i,at in enumerate(range(0,len(blob),137)):(self.source/f'{i:02}').write_bytes(blob[at:at+137])
        self.run_cache();self.assertEqual((self.cache/hashlib.sha256(data).hexdigest()/'0000.bin').read_bytes(),data)
    def test_compressed_input_chunk_boundaries(self):
        data=hashlib.shake_256(b'N Launcher streaming boundary fixture').digest(2*1048576+111)
        self.source.write_bytes(fixture(data,raw=True));self.run_cache();folder=self.cache/hashlib.sha256(data).hexdigest()
        self.assertEqual(b''.join(p.read_bytes() for p in sorted(folder.glob('*.bin'))),data)
    def test_bad_content_has_no_complete_marker(self):
        self.source.write_bytes(fixture(b'B'*1024,corrupt=True));self.run_cache(ok=False);self.assertFalse(list(self.cache.glob('*/complete')))
    def test_cancel_and_resume(self):
        data=b'C'*200000;self.source.write_bytes(fixture(data));self.run_cache('cancel',ok=False);self.assertFalse(list(self.cache.glob('*/complete')));self.run_cache();self.assertTrue(list(self.cache.glob('*/complete')))
    def test_reject_trailing_bytes(self):
        self.source.write_bytes(fixture(b'T'*200000,trailing=True));self.run_cache(ok=False);self.assertFalse(list(self.cache.glob('*/complete')))
    def test_missing_cached_part_fails_without_overwrite(self):
        data=b'M'*200000;self.source.write_bytes(fixture(data));self.run_cache();folder=self.cache/hashlib.sha256(data).hexdigest();(folder/'0000.bin').unlink();self.run_cache(ok=False);self.assertTrue((folder/'complete').exists())
if __name__=='__main__':unittest.main()
