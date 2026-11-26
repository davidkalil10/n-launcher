"""Tiny, synthetic NUPs for UI previews and split library tests. No game data."""
from pathlib import Path
import hashlib, io, struct, sys
from PIL import Image, ImageDraw

def fixture(title,index):
    im=Image.new('RGB',(256,256));p=im.load()
    colors=[(42,98,108),(113,65,107),(110,86,41),(56,86,134),(66,110,64),(112,66,49)]
    r,g,b=colors[index%len(colors)]
    for y in range(256):
        for x in range(256):p[x,y]=(min(255,int(r*(.6+y/256))),min(255,int(g*(.6+y/256))),min(255,int(b*(.6+y/256))))
    d=ImageDraw.Draw(im);d.ellipse((100,-60,320,160),fill=(r+50,g+50,b+50));d.polygon([(0,225),(60,80),(150,240),(200,120),(256,235),(256,256),(0,256)],fill=(20,35,42))
    icon=io.BytesIO();im.save(icon,format='JPEG',quality=85)
    facts=[(1,b'META'+bytes(124)),(3,icon.getvalue()),(4,b'1.0.0'),(5,title.encode()),(6,b'N Launcher / biblioteca de demonstracao')]
    meta=b''.join(struct.pack('<HHI',kind,0,len(data))+data for kind,data in facts)
    length=0x30+0x40+0x18
    pkg=bytearray(0x40);struct.pack_into('<QIIII',pkg,0,0x0400F00000001000+(index<<16),0,0xffffffff,0xffffffff,0);struct.pack_into('<QI',pkg,0x20,0x80+length,len(meta))
    toc=bytearray(0x30);toc[:4]=b'TOC\1';struct.pack_into('<IQQQQ',toc,4,1,1,0,0,length)
    # string-pool offset/size are at +24/+32; no names, no children.
    struct.pack_into('<QQ',toc,24,length,0)
    toc=bytes(toc)+pkg+bytes(0x18)
    head=bytearray(0x80);head[:4]=b'NUP\1';struct.pack_into('<QQ',head,8,0x80,len(toc));head[24:56]=hashlib.sha256(toc).digest();struct.pack_into('<QQI',head,0x38,0x80+len(toc),0x80+len(toc)+len(meta),1)
    return bytes(head)+toc+meta

def main(root):
    folder=root/'switch/n-launcher/input';folder.mkdir(parents=True,exist_ok=True)
    for i,title in enumerate(['Aurora Valley','Café Nebula','Clockwork Islands','Lantern Harbor','Paper Comet','Quiet Tides']):
        data=fixture(title,i)
        path=folder/(title+'.nup')
        if i in [1,4]:
            path.mkdir(exist_ok=True);at=len(data)//2
            (path/'00').write_bytes(data[:at]);(path/'01').write_bytes(data[at:])
        else:path.write_bytes(data)
    print('Six synthetic NUPs; two split directories.')
if __name__=='__main__':main(Path(sys.argv[1]))
