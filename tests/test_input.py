"""Synthetic fixtures only: exercise the exact same reader used by the NRO."""
import hashlib, pathlib, random, struct, subprocess, sys, tempfile, unittest

EXE=pathlib.Path(sys.argv.pop(1)).resolve()
def fixture(extra=b''):
    toc=bytearray(0x30+0x40+0x18)
    toc[:4]=b'TOC\1'
    struct.pack_into('<IQQQQ',toc,4,1,1,0,len(toc),0)
    struct.pack_into('<QIIIIBB',toc,0x30,0x0400000000000000,0,0xffffffff,0xffffffff,1,0,0)
    h=bytearray(0x80);h[:4]=b'NUP\1'
    struct.pack_into('<QQ',h,8,0x80,len(toc));h[0x18:0x38]=hashlib.sha256(toc).digest()
    struct.pack_into('<QQI',h,0x38,len(h)+len(toc),len(h)+len(toc)+len(extra),1)
    return bytes(h+toc)+extra

class Reader(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def run_probe(self,path,*args,ok=True):
        p=subprocess.run([str(EXE),str(path),*map(str,args)],capture_output=True,text=True)
        self.assertEqual(p.returncode,0 if ok else 1,p.stdout+p.stderr);return p.stdout
    def split(self,blob,step=37):
        p=self.root/'jogo.nup';p.mkdir()
        for i,at in enumerate(range(0,len(blob),step)):(p/f'{i:02}').write_bytes(blob[at:at+step])
        return p
    def test_unicode_path(self):
        folder=self.root/'Pokémon - 日本語';folder.mkdir()
        p=folder/'Jogo.nup';p.write_bytes(fixture());self.run_probe(p)
    def test_single_and_split_same_inventory(self):
        blob=fixture();p=self.root/'one.nup';p.write_bytes(blob)
        self.assertIn('packages=1',self.run_probe(p));self.assertIn('packages=1',self.run_probe(self.split(blob)))
    def test_reads_across_many_boundaries(self):
        blob=random.Random(42).randbytes(900);p=self.split(blob)
        for offset,count in [(0,0),(36,2),(35,400),(900,0),(899,1)]:
            self.assertEqual(self.run_probe(p,'--read',offset,count).strip(),blob[offset:offset+count].hex())
        self.run_probe(p,'--read',899,2,ok=False)
    def test_missing_middle(self):
        p=self.split(fixture());(p/'01').unlink();self.run_probe(p,ok=False)
    def test_missing_final(self):
        p=self.split(fixture());sorted(p.iterdir())[-1].unlink();self.run_probe(p,ok=False)
    def test_missing_zero(self):
        p=self.split(fixture());(p/'00').unlink();self.run_probe(p,ok=False)
    def test_empty_part(self):
        p=self.split(fixture());(p/'01').write_bytes(b'');self.run_probe(p,ok=False)
    def test_bad_digest(self):
        b=bytearray(fixture());b[-1]^=1;p=self.root/'bad.nup';p.write_bytes(b);self.run_probe(p,ok=False)
    def test_huge_toc(self):
        b=bytearray(fixture());struct.pack_into('<Q',b,16,2**64-1)
        p=self.root/'bad.nup';p.write_bytes(b);self.run_probe(p,ok=False)
    def test_renamed_nsp(self):
        p=self.root/'bad.nup';p.write_bytes(b'PFS0'+bytes(256));self.run_probe(p,ok=False)
    def test_suffixed_parts(self):
        blob=fixture()
        for i,at in enumerate(range(0,len(blob),47)):(self.root/f'game.nup.{i:02}').write_bytes(blob[at:at+47])
        self.assertIn('packages=1',self.run_probe(self.root/'game.nup.01'))
    def test_hash_padding_boundaries(self):
        for size in (0,1,7,8,9,55,56,63,64,65,100):
            b=bytearray(fixture());toc=bytes(b[0x80:])+bytes(size)
            struct.pack_into('<Q',b,16,len(toc));b[0x18:0x38]=hashlib.sha256(toc).digest()
            struct.pack_into('<QQ',b,0x38,0x80+len(toc),0x80+len(toc))
            p=self.root/'hash.nup';p.write_bytes(b[:0x80]+toc);self.run_probe(p)

if __name__=='__main__':unittest.main()
