"""Run the actual native backend against mock FS, including cache/part boundaries and bounds."""
import hashlib,pathlib,struct,subprocess,sys,tempfile,unittest
EXE=pathlib.Path(sys.argv.pop(1)).resolve()
def nup(payload):
    toc=bytearray(0x88);toc[:4]=b'TOC\1';struct.pack_into('<IQQQQ',toc,4,1,1,0,len(toc),0)
    struct.pack_into('<QIIIIBB',toc,0x30,0x0400F00000001000,0,0xffffffff,0xffffffff,0,0,0)
    head=bytearray(128);head[:4]=b'NUP\1';struct.pack_into('<QQ',head,8,128,len(toc));head[24:56]=hashlib.sha256(toc).digest();struct.pack_into('<QQI',head,56,128+len(toc),128+len(toc)+len(payload),1)
    return bytes(head+toc)+payload
class Native(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def call(self,*args,ok=True):
        p=subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True);self.assertEqual(p.returncode,0 if ok else 1,p.stdout+p.stderr);return bytes.fromhex(p.stdout) if ok else b''
    def test_single_and_split_boundary(self):
        blob=nup(bytes(range(256))*8);single=self.root/'single.nup';single.write_bytes(blob)
        split=self.root/'split.nup';split.mkdir();(split/'00').write_bytes(blob[:713]);(split/'01').write_bytes(blob[713:])
        for path in [single,split]:self.assertEqual(self.call('input',path,700,50),blob[700:750])
    @unittest.skipIf(sys.platform=='win32','Sparse-file allocation is tested on Linux to avoid filling a Windows disk')
    def test_boundary_above_four_gib(self):
        split=self.root/'large.nup';split.mkdir();boundary=(1<<32)+1024;tail=b'abcdefghijklmnop';blob=bytearray(nup(b''));struct.pack_into('<Q',blob,64,boundary+len(tail))
        with (split/'00').open('wb') as f:f.write(blob);f.seek(boundary-8);f.write(b'12345678')
        (split/'01').write_bytes(tail)
        self.assertEqual(self.call('input',split,boundary-4,8),b'5678abcd')
    def test_reject_past_end(self):
        path=self.root/'one.nup';blob=nup(b'ab');path.write_bytes(blob);self.call('input',path,len(blob)-1,2,ok=False)
    def test_cached_part_boundary_and_bounds(self):
        data=bytes(range(256))*10;sha=hashlib.sha256(data).hexdigest();folder=self.root/'switch/n-launcher/content'/sha;folder.mkdir(parents=True)
        for i,at in enumerate(range(0,len(data),1024)):(folder/f'{i:04}.bin').write_bytes(data[at:at+1024])
        (folder/'complete').write_bytes(b'NLC1'+struct.pack('<IQ',1024,len(data))+bytes.fromhex(sha))
        self.assertEqual(self.call('cache',self.root,sha,len(data),1000,80),data[1000:1080]);self.call('cache',self.root,sha,len(data),len(data)-1,2,ok=False)
        (folder/'0001.bin').unlink();self.call('cache',self.root,sha,len(data),0,10,ok=False)
if __name__=='__main__':unittest.main()
