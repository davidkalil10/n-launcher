#!/usr/bin/env python3
"""N Launcher GUI build and previews, using the same frontend sources."""
import argparse, os, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from prepare_flow import prepare
from bake_ui_font import bake

def main():
    p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--preview',action='store_true');p.add_argument('--sdk');p.add_argument('--support',default='');p.add_argument('--diagnostic',action='store_true');p.add_argument('--allocator',choices=['newlib','mimalloc'],default='newlib');a=p.parse_args()
    prepare();bake(ROOT/'build/flow/menu/source/font_baked.h');os.chdir(ROOT)
    env=dict(os.environ);env['PATH']=str(Path(a.compiler).parent)+os.pathsep+a.support+os.pathsep+env.get('PATH','')
    def run(args):subprocess.run([str(s) for s in args],env=env,check=True)
    common=['src/gui.cpp','build/flow/menu/source/nup.cpp','build/flow/menu/source/library.cpp','build/flow/menu/source/images.cpp','src/input.cpp','src/profile.cpp']
    flags=['-std=gnu++20','-fno-char8_t','-O2','-Wall','-Iinclude','-Isrc','-Ibuild/flow/menu/source']
    out=ROOT/('build/gui-diagnostic' if a.diagnostic else 'build/gui');out.mkdir(parents=True,exist_ok=True)
    if a.diagnostic and a.preview:p.error('--diagnostic is a native build option')
    if a.diagnostic:flags+=['-DNLAUNCHER_DIAGNOSTIC','-g']
    if a.preview:
        exe=out/'preview.exe'
        run([a.compiler,*flags,'-static','-static-libgcc','-static-libstdc++',*common,'src/gui_preview.cpp','-o',exe])
        card=out/'card';images=out/'preview'
        run([sys.executable,'tools/make_ui_fixtures.py',card])
        test=out/'gui-test.exe'
        run([a.compiler,*flags,'-static','-static-libgcc','-static-libstdc++',*common,'tests/gui_test.cpp','-o',test])
        run([test,card])
        run([exe,card,images])
        from PIL import Image
        for ppm in images.glob('*.ppm'):
            with Image.open(ppm) as image:image.save(ppm.with_suffix('.png'))
        print(images)
    else:
        if not a.sdk:p.error('--sdk required for Switch 2')
        sdk=Path(a.sdk).as_posix();env['DEVKITPRO']=sdk
        elf=out/'n-launcher.elf';mapfile=out/'n-launcher.map'
        flags+=['-fPIE','-ffunction-sections','-fdata-sections','-mcpu=cortex-a78c+crypto','-mtp=soft','-ftls-model=local-exec','-D__SWITCH__','-D__SWITCH2__',f'-specs={sdk}/libounce/switch.specs','-isystem',f'{sdk}/libounce/include','-Ithird_party/zstd','-Ibuild/generated']
        # The single user-facing NRO carries its own generic loader; no game-specific build.
        blob=out/'runtime_blob.S';blob.write_text('.section .rodata.nl_runtime,"a"\n.balign 16\n.global nl_runtime_begin\n.global nl_runtime_end\nnl_runtime_begin:\n.incbin "dist/n-launcher-game.nro"\nnl_runtime_end:\n',encoding='ascii')
        obj=out/'runtime_blob.o';cc=Path(a.compiler).with_name('aarch64-none-elf-gcc.exe')
        run([cc,'-c',blob,'-o',obj])
        run([a.compiler,*flags,*common,'src/program.cpp','src/cache.cpp','src/loader_probe.cpp','src/gui_switch.cpp','src/launch_trace.cpp','src/fatal_trace.cpp','src/update.cpp','src/http.cpp','src/https_switch.cpp','src/layout.cpp','build/switch2/zstd.o',obj,f'-L{sdk}/libounce/lib',f'-L{sdk}/portlibs/switch2/lib','-lkaihen','-lounce_core' if a.allocator=='newlib' else '-lnx','-Wl,--gc-sections',f'-Wl,-Map,{mapfile}','-o',elf])
        map_text=mapfile.read_text(errors='replace')
        if a.allocator=='newlib' and ('libounce_core.a' not in map_text or 'malloc_init.o' in map_text or '__mi_theap_default' in map_text):
            raise RuntimeError('Frontend allocator does not match requested newlib baseline')
        print('Frontend allocator:',a.allocator)
        version=(ROOT/'include/nx2link/version.hpp').read_text().split('"')[1]
        run([f'{sdk}/tools/bin/nacptool.exe','--create','N Launcher - Diagnostic' if a.diagnostic else 'N Launcher','David Kalil Braga',version,out/'app.nacp'])
        from PIL import Image,ImageDraw,ImageFont
        icon=Image.new('RGB',(256,256),(12,17,25));draw=ImageDraw.Draw(icon);draw.rounded_rectangle((28,28,228,228),radius=56,fill=(80,226,184))
        font=ImageFont.truetype(str(ROOT/'third_party/noto-sans/NotoSans.ttf'),164);draw.text((128,119),'N',font=font,anchor='mm',fill=(12,17,25));icon.save(out/'icon.jpg',quality=95)
        nro=ROOT/('dist/n-launcher-diagnostic.nro' if a.diagnostic else 'dist/n-launcher.nro')
        run([f'{sdk}/tools/bin/elf2nro.exe',elf,nro,f'--nacp={out}/app.nacp',f'--icon={out}/icon.jpg'])
        run([sys.executable,'tools/verify_switch2_package.py',nro,mapfile])
        if a.diagnostic:
            import hashlib,json,zipfile
            with zipfile.ZipFile(ROOT/'dist/n-launcher-diagnostic-symbols.zip','w',zipfile.ZIP_DEFLATED) as symbols:
                symbols.write(elf,elf.name);symbols.write(mapfile,mapfile.name)
                symbols.writestr('build.json',json.dumps({'version':version,'sha256':hashlib.sha256(nro.read_bytes()).hexdigest(),'allocator':a.allocator}))
        print(nro)
if __name__=='__main__':main()
