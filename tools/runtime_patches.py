# SPDX-License-Identifier: MIT
"""Reviewed runtime adaptations over config/flow.json (nx2-flow 0.2.6 / loader 0.1.7).

Upstream 0.2.6 already includes PR #44 priority-lock backoff (spinlock.h), HeapDef.byml
fixed-heaps detection, large NSO BSS allocation in nx2_big_at, IFileSystem cmd 16
(GetFileSystemAttribute), AOC (kind=2), SD-backed cache storage, cross-title read-only
save migration, bcat_offline, and preselected user capture.

This module applies only N Launcher's own reviewed integration points:
- Brand/paths (/switch/n-launcher/...)
- Shared split-NUP Input backend (nl_input_open / nl_input_size / nl_input_read)
- >64 MiB SD-backed RomFS cache (kind=3 via nl_cache_open / nl_cache_read)
- 1536 MiB initial loader room (returned to kernel after open) for >1 GiB static BSS titles
"""
def patch_runtime(out):
    def patch(file,before,after):
        p=out/'loader/runtime'/file;s=p.read_text(encoding='utf-8')
        if s.count(before)!=1:raise RuntimeError(f'Runtime patch no longer applies: {file}: {before[:70]}')
        p.write_text(s.replace(before,after),encoding='utf-8',newline='\n')
    patch('nx2.h','#define NX2_TOOL "nx2-flow"','#define NX2_TOOL "switch/n-launcher"')
    patch('game.c','#include "nx2.h"','#include "nx2.h"\nint nl_input_open(const char*);\nu64 nl_input_size(void);\nu32 nl_input_read(u64,void*,u64);\nint nl_cache_open(const u8*,u64);\nu32 nl_cache_read(u32,u64,void*,u64);')
    p=out/'loader/runtime/game.c';s=p.read_text(encoding='utf-8')
    a=s.index('    return nx2_fs_read_parts',s.index('static u32 nup_read'));b=s.index('\n}\n',a)
    s=s[:a]+'    return nl_input_read(at,to,size);'+s[b:]
    a=s.index('    FsFileSystem *sd =',s.index('static int nup_open'));b=s.index('    u8 head[0x80];',a)
    s=s[:a]+'''    nup.sd = fsdevGetDeviceFileSystem("sdmc");
    if (nl_input_open(path)) {
        nx2_log("[nlauncher] NUP/split validation or native open failed\\n");
        return -1;
    }
    nup.size = nl_input_size();
    nup.nparts = 1;
'''+s[b:]
    # The optional single-file benchmark bypasses the shared reader; do not enable it in our loader.
    a=s.index('static void read_test(void)');b=s.index('// How far the loader',a)
    s=s[:a]+'static void read_test(void) {} // diagnostic benchmark omitted; all payload reads go through Input.\n\n'+s[b:]
    s=s.replace('"/nx2-flow/unknown"','"/switch/n-launcher/unknown"').replace('strcpy(home, "/nx2-flow/")','strcpy(home, "/switch/n-launcher/")').replace('mkdir("sdmc:/nx2-flow", 0777);','mkdir("sdmc:/switch/n-launcher", 0777);')
    # Cached main-RomFS files (>64 MiB compressed) become kind 3 and never pass through the whole-file RAM unpacker.
    before='    {  // (files kept packed: the thread that unpacks them as they are read, across the game and its add-ons)'
    if s.count(before)!=1:raise RuntimeError('Runtime patch no longer applies: game.c unpacker anchor')
    s=s.replace(before,'''    for (u32 k=0;k<roms[0].nfiles;k++) {
        Ent *e=&roms[0].files[k];
        if(e->kind!=1 || e->size<=(64ull<<20))continue;
        const u8 *sha=NULL;
        for(u64 f=0;f<nup.nfiles;f++)if(nup_file(f).at==e->at){sha=nup.files+0x50*f+32;break;}
        const int cached=sha?nl_cache_open(sha,e->size):-1;
        if(cached<0){nx2_log("[nlauncher] large-file cache missing/invalid; prepare this NUP in N Launcher first\\n");return -1;}
        e->kind=3;e->at=(u64)cached;
    }
'''+before)
    read_anchor='                } else if (e->kind == 2) {'
    if s.count(read_anchor)!=1:raise RuntimeError('Runtime patch no longer applies: game.c kind==2 anchor')
    s=s.replace(read_anchor,'                } else if (e->kind == 3) {\n                    rc = nl_cache_read((u32)e->at,in-e->data,to,take);\n                } else if (e->kind == 2) {',1)
    p.write_text(s,encoding='utf-8',newline='\n')
    # Start with 1536 MiB initial room so titles with >1 GiB static .bss work on Automatic; unused room is returned to the kernel.
    patch('main.c','            u64 room = 1024;','            u64 room = 1536;')
    # Both fallback and normal return paths point to our app, without changing activation.
    for p in (out/'loader/runtime').glob('*.c'):
        s=p.read_text(encoding='utf-8');s=s.replace('sdmc:/switch/nx2-flow/nx2-flow.nro','sdmc:/switch/n-launcher/n-launcher.nro').replace('sdmc:/nx2-flow/next.txt','sdmc:/switch/n-launcher/next.txt')
        p.write_text(s,encoding='utf-8',newline='\n')
