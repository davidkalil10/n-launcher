# SPDX-License-Identifier: GPL-2.0-or-later
"""Bake OFL Noto Sans glyphs into the nx2-flow canvas format, including Portuguese accents."""
from pathlib import Path
import hashlib
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
FONT=ROOT/'third_party/noto-sans/NotoSans.ttf'
SHA='bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d'
def bake(out):
    if hashlib.sha256(FONT.read_bytes()).hexdigest()!=SHA:raise RuntimeError('Noto Sans font hash mismatch')
    font=ImageFont.truetype(str(FONT),48)
    glyphs=[];blob=bytearray()
    for cp in list(range(32,127))+list(range(160,256))+[0x2013,0x2014,0x2018,0x2019,0x201c,0x201d,0x2022,0x2026,0x2212]:
        mask=font.getmask(chr(cp));w,h=mask.size;box=font.getbbox(chr(cp),anchor='ls');advance=round(font.getlength(chr(cp)))
        if cp in [32,160]:w=h=0
        glyphs.append((cp,w,h,box[0],-box[1],advance,len(blob)))
        if w and h:blob+=bytes(mask)
    with Path(out).open('w',encoding='utf-8',newline='\n') as f:
        f.write('// Glyph data generated from Noto Sans (SIL Open Font License 1.1).\n// See third_party/noto-sans/OFL.txt. Rebuild with tools/bake_ui_font.py.\n#pragma once\n#include <cstdint>\nnamespace BakedFont {\nconstexpr int kBase=48;\nstruct Glyph {std::uint32_t cp;std::uint8_t w,h;std::int8_t left,top;std::uint8_t advance;std::uint32_t at;};\n')
        f.write(f'constexpr int kCount={len(glyphs)};\ninline constexpr Glyph kGlyphs[kCount]={{\n')
        for g in glyphs:f.write('{'+','.join(map(str,g))+'},\n')
        f.write('};\ninline constexpr std::uint8_t kCoverage[]={\n')
        for i in range(0,len(blob),40):f.write(','.join(map(str,blob[i:i+40]))+',\n')
        f.write('};\n}\n')
if __name__=='__main__':
    import sys
    bake(sys.argv[1])
