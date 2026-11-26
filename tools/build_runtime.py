#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the pinned generic runtime with our native multipart/cache backend; no game inputs."""
from pathlib import Path
import argparse,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from prepare_flow import prepare
def main():
    p=argparse.ArgumentParser();p.add_argument('--sdk',required=True);p.add_argument('--support',default='');a=p.parse_args()
    out=prepare();sdk=Path(a.sdk);runtime=out/'loader/runtime';build=ROOT/'build/runtime';build.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,DEVKITPRO=sdk.as_posix());env['PATH']=str(sdk/'devkitA64/bin')+os.pathsep+a.support+os.pathsep+env.get('PATH','')
    def run(args,cwd=ROOT):subprocess.run([str(s) for s in args],env=env,cwd=cwd,check=True)
    cc=sdk/'devkitA64/bin/aarch64-none-elf-gcc.exe';cpp=sdk/'devkitA64/bin/aarch64-none-elf-g++.exe'
    arch=['-mcpu=cortex-a78c+crypto','-mtp=soft','-fPIE']
    flags=['-O2','-g','-Wall','-ffunction-sections','-fdata-sections',*arch,'-D__SWITCH__','-D__SWITCH2__',f'-I{sdk.as_posix()}/libounce/include',f'-I{sdk.as_posix()}/portlibs/switch2/include',f'-I{ROOT.as_posix()}/include',f'-I{runtime.as_posix()}',f'-I{ROOT.as_posix()}/third_party/zstd']
    version=(ROOT/'include/nx2link/version.hpp').read_text().split('"')[1]
    loader_version=json.loads((ROOT/'config/flow.json').read_text())['loaderVersion']
    defines=['-DNX2_LOADER']+[f'-DNX2_GAME_{key}=nx2_game_{key.lower()}' for key in ['MB','ROOM_MB','APART_MB','REMOTE_MB','SYSRES_KB']]+[f'-DNX2_VERSION="N-Launcher-{version}-flow-{loader_version}"']
    objects=[]
    for source in sorted(runtime.glob('*')):
        if source.suffix not in ['.c','.s']:continue
        obj=build/(source.name+'.o');run([cc,'-c',*flags,*defines,source,'-o',obj],runtime);objects.append(obj)
    z=build/'zstd.o';run([cc,'-c',*flags,'-DZSTD_DISABLE_ASM',ROOT/'third_party/zstd/zstddeclib.c','-o',z]);objects.append(z)
    for source in [ROOT/'src/native_input.cpp',ROOT/'src/input.cpp']:
        obj=build/(source.name+'.o');run([cpp,'-c','-std=c++17',*flags,source,'-o',obj]);objects.append(obj)
    elf=build/'n-launcher-game.elf';link_map=build/'n-launcher-game.map'
    run([cpp,*arch,f'-specs={sdk.as_posix()}/libounce/switch.specs',*objects,f'-L{sdk.as_posix()}/libounce/lib',f'-L{sdk.as_posix()}/portlibs/switch2/lib','-lEGL','-lGLESv2','-lglapi','-ldrm_nouveau','-lnx','-Wl,--gc-sections','-Wl,--wrap=__libnx_exception_entry','-Wl,--wrap=svcBreak,--wrap=svcExitProcess,--wrap=svcReturnFromException,--wrap=abort,--wrap=svcExitThread',f'-Wl,-Map,{link_map}','-o',elf])
    run([sdk/'tools/bin/nacptool.exe','--create','N Launcher - Game Loader','David Kalil Braga / Olmectron',version,build/'app.nacp'])
    nro=ROOT/'dist/n-launcher-game.nro';nro.parent.mkdir(exist_ok=True)
    run([sdk/'tools/bin/elf2nro.exe',elf,nro,f'--nacp={build}/app.nacp',f'--icon={sdk.as_posix()}/libounce/default_icon.jpg'])
    run([sys.executable,ROOT/'tools/verify_switch2_package.py',nro,link_map]);print(nro)
if __name__=='__main__':main()
