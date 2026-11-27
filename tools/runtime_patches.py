# SPDX-License-Identifier: MIT
"""Reviewed runtime adaptations over config/flow.json. Includes the pinned PR44 lock-backoff adaptation; TLS and game-code policies stay upstream."""
def patch_runtime(out):
    def patch(file,before,after):
        p=out/'loader/runtime'/file;s=p.read_text(encoding='utf-8')
        if s.count(before)!=1:raise RuntimeError(f'Runtime patch no longer applies: {file}: {before[:70]}')
        p.write_text(s.replace(before,after),encoding='utf-8',newline='\n')
    patch('nx2.h','#define NX2_TOOL "nx2-flow"','#define NX2_TOOL "switch/n-launcher"')
    patch('game.c','#include "nx2.h"','#include "nx2.h"\nint nl_input_open(const char*);\nu32 nl_input_read(u64,void*,u64);\nint nl_cache_open(const u8*,u64);\nu32 nl_cache_read(u32,u64,void*,u64);')
    p=out/'loader/runtime/game.c';s=p.read_text(encoding='utf-8');a=s.index('    u8 *out = to;',s.index('static u32 nup_read'));b=s.index('\n}\n',a)
    s=s[:a]+'    return nl_input_read(at,to,size);'+s[b:]
    a=s.index('    FsFileSystem *sd =',s.index('static int nup_open'));b=s.index('    u8 head[0x80];',a)
    s=s[:a]+'''    if (nl_input_open(path)) {
        nx2_log("[nlauncher] NUP/split validation or native open failed\\n");
        return -1;
    }
'''+s[b:]
    # The optional single-file benchmark bypasses the shared reader; do not enable it in our loader.
    a=s.index('static void read_test(void)');b=s.index('// How far the loader',a)
    s=s[:a]+'static void read_test(void) {} // diagnostic benchmark omitted; all payload reads go through Input.\n\n'+s[b:]
    s=s.replace('"/nx2-flow/unknown"','"/switch/n-launcher/unknown"').replace('strcpy(home, "/nx2-flow/")','strcpy(home, "/switch/n-launcher/")').replace('mkdir("sdmc:/nx2-flow", 0777);','mkdir("sdmc:/switch/n-launcher", 0777);')
    # Cached files become kind 3 and never pass through the whole-file RAM unpacker.
    before='    {  // (files kept packed: the thread that unpacks them as they are read)'
    assert s.count(before)==1
    s=s.replace(before,'''    for (u32 k=0;k<rom.nfiles;k++) {
        Ent *e=&rom.files[k];
        if(e->kind!=1 || e->size<=(64ull<<20))continue;
        const u8 *sha=NULL;
        for(u64 f=0;f<nup.nfiles;f++)if(nup_file(f).at==e->at){sha=nup.files+0x50*f+32;break;}
        const int cached=sha?nl_cache_open(sha,e->size):-1;
        if(cached<0){nx2_log("[nlauncher] large-file cache missing/invalid; prepare this NUP in N Launcher first\\n");return -1;}
        e->kind=3;e->at=(u64)cached;
    }
'''+before)
    s=s.replace('                } else if (e->kind == 2) {','                } else if (e->kind == 3) {\n                    rc = nl_cache_read((u32)e->at,in-e->data,to,take);\n                } else if (e->kind == 2) {',1)
    p.write_text(s,encoding='utf-8',newline='\n')
    patch_priority_locks(out)
    # Both fallback and normal return paths point to our app, without changing activation.
    for p in (out/'loader/runtime').glob('*.c'):
        s=p.read_text(encoding='utf-8');s=s.replace('sdmc:/switch/nx2-flow/nx2-flow.nro','sdmc:/switch/n-launcher/n-launcher.nro').replace('sdmc:/nx2-flow/next.txt','sdmc:/switch/n-launcher/next.txt')
        p.write_text(s,encoding='utf-8',newline='\n')


def patch_priority_locks(out):
    """Backport reviewed PR44 without importing unrelated runtime changes."""
    from pathlib import Path
    import hashlib, json, re
    root=Path(__file__).resolve().parents[1]
    pin=json.loads((root/'config/runtime-lock-fix.json').read_text(encoding='utf-8'))
    header=(root/pin['file']).read_bytes()
    if hashlib.sha256(header).hexdigest()!=pin['sha256']:
        raise RuntimeError('Pinned priority-lock helper changed')
    expected={'game.c':7,'audio2.c':1,'rom.c':3,'svc.c':1}
    for name,count in expected.items():
        p=out/'loader/runtime'/name
        s=p.read_text(encoding='utf-8')
        s=s.replace('#include "nx2.h"','#include "nx2.h"\n#include "nx2link/upstream_spinlock.h"',1)
        pattern=r'while \(__atomic_exchange_n\(([^,]+), 1, __ATOMIC_ACQUIRE\)\) svcSleepThread\(0\);'
        s,n=re.subn(pattern,r'nx2_spin_lock(\1);',s)
        if n!=count:raise RuntimeError(f'Priority-lock patch drift: {name}: {n} != {count}')
        p.write_text(s,encoding='utf-8',newline='\n')
    p=out/'loader/runtime/nvnwatch.c';s=p.read_text(encoding='utf-8')
    s=s.replace('#include "nx2.h"','#include "nx2.h"\n#include "nx2link/upstream_spinlock.h"',1)
    before='while (!__atomic_compare_exchange_n(&owner, &none, me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) none = 0, svcSleepThread(0);'
    after='u32 spins = 0;\n        while (!__atomic_compare_exchange_n(&owner, &none, me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) { none = 0; nx2_spin_backoff(&spins); }'
    if s.count(before)!=1:raise RuntimeError('Priority-lock patch drift: nvnwatch.c')
    p.write_text(s.replace(before,after),encoding='utf-8',newline='\n')
    for name in [*expected,'nvnwatch.c']:
        if 'svcSleepThread(0)' in (out/'loader/runtime'/name).read_text(encoding='utf-8'):
            raise RuntimeError(f'Unreviewed zero-sleep wait remains: {name}')
