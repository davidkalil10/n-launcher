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
    patch_heapdef_fixed_heaps(out)
    patch_priority_locks(out)
    # Both fallback and normal return paths point to our app, without changing activation.
    for p in (out/'loader/runtime').glob('*.c'):
        s=p.read_text(encoding='utf-8');s=s.replace('sdmc:/switch/nx2-flow/nx2-flow.nro','sdmc:/switch/n-launcher/n-launcher.nro').replace('sdmc:/nx2-flow/next.txt','sdmc:/switch/n-launcher/next.txt')
        p.write_text(s,encoding='utf-8',newline='\n')


def patch_heapdef_fixed_heaps(out):
    """Backport upstream nx2-flow 0.2.6 HeapDef.byml fixed-heaps detection and VAM trim."""
    def patch(file,before,after):
        p=out/'loader/runtime'/file;s=p.read_text(encoding='utf-8')
        if s.count(before)!=1:raise RuntimeError(f'HeapDef patch drift: {file}: {before[:70]}')
        p.write_text(s.replace(before,after),encoding='utf-8',newline='\n')
    patch('nx2.h',
          'extern u32 nx2_game_mb, nx2_game_room_mb, nx2_game_apart_mb, nx2_game_remote_mb, nx2_game_sysres_kb;',
          'extern u32 nx2_game_mb, nx2_game_room_mb, nx2_game_apart_mb, nx2_game_fixed_mb, nx2_game_auto_heaps, nx2_game_remote_mb, nx2_game_sysres_kb;')
    patch('game.c',
          'u32 nx2_game_mb = 8192, nx2_game_room_mb = 400, nx2_game_apart_mb, nx2_game_remote_mb, nx2_game_sysres_kb;',
          'u32 nx2_game_mb = 8192, nx2_game_room_mb = 400, nx2_game_apart_mb, nx2_game_fixed_mb, nx2_game_auto_heaps, nx2_game_remote_mb, nx2_game_sysres_kb;')
    patch('game.c',
          'static void heaps_read(void) {',
          '''// (a BYML integer entry decoded to signed KB: 1 if an integer type, 0 otherwise)
static int byml_size_kb(const u8 *b, u64 size, u32 kind, u32 v, s64 *out) {
    if (kind == 0xD1) { *out = (s32)v; return 1; }
    if (kind == 0xD3) { *out = (s64)(u64)v; return 1; }
    if (kind == 0xD4 && (u64)v + 8 <= size) { *out = (s64)r64(b + v); return 1; }
    if (kind == 0xD5 && (u64)v + 8 <= size) {
        const u64 raw = r64(b + v);
        if (raw > (u64)INT64_MAX) return 0;
        *out = (s64)raw;
        return 1;
    }
    return 0;
}
// (walk a HeapDef.byml Heaps map once: VirtualAddressMemory Size in KB, sum of top-level fixed heaps, and count of non-Reserved auto-sized top-level heaps)
static void byml_scan_heaps(const u8 *b, u64 size, u64 map, u64 *vam_kb, u64 *fixed_kb, u32 *auto_heaps) {
    const u64 keys = r32(b + 4);
    if (map + 4 > size || b[map] != 0xC1 || keys + 4 > size || b[keys] != 0xC2) return;
    const u32 n = r32(b + map) >> 8, nkeys = r32(b + keys) >> 8;
    for (u32 k = 0; k < n && map + 12 + 8ull * k <= size; k++) {
        const u32 w = r32(b + map + 4 + 8 * k), entry_kind = w >> 24, index = w & 0xFFFFFF;
        if (entry_kind != 0xC1 || index >= nkeys || keys + 8 + 4ull * index > size) continue;
        const u64 at = keys + r32(b + keys + 4 + 4 * index);
        if (at >= size) continue;
        const u32 entry_val = r32(b + map + 8 + 8 * k);
        u32 sv = 0, pv = 0;
        const u32 sk = byml_get(b, size, entry_val, "Size", &sv);
        s64 sz_kb = 0;
        if (!byml_size_kb(b, size, sk, sv, &sz_kb)) continue;
        const int is_vam = (size - at > 20) && !memcmp(b + at, "VirtualAddressMemory", 21);
        if (is_vam) {
            if (sz_kb > 0) *vam_kb = (u64)sz_kb;
        } else if (!byml_get(b, size, entry_val, "Parent", &pv)) {
            if (sz_kb > 0 && (u64)sz_kb < (64ull << 20)) {
                *fixed_kb += (u64)sz_kb;
            } else if (sz_kb < 0) {
                const int is_reserved = (size - at > 8) && !memcmp(b + at, "Reserved", 9);
                if (!is_reserved) (*auto_heaps)++;
            }
        }
    }
}
static void heaps_read(void) {''')
    patch('game.c',
          '''    u32 v = 0, kind = 0;
    if (byml && size > 16 && byml[0] == 'Y' && byml[1] == 'B') {
        const u64 root = r32(byml + 12);
        if (byml_get(byml, size, root, "Heaps", &v) == 0xC1 && byml_get(byml, size, v, "VirtualAddressMemory", &v) == 0xC1) kind = byml_get(byml, size, v, "Size", &v);
    }
    u64 kb = 0;
    if (kind == 0xD1 || kind == 0xD3) kb = v;
    else if ((kind == 0xD4 || kind == 0xD5) && (u64)v + 8 <= size) kb = r64(byml + v);
    if (!byml) nx2_log("[game] the game's table of heaps (%u KB) couldn't be unpacked: it is told of memory as a game that has none\\n", table->size >> 10);
    else if (kb >= 1024 && kb < (16ull << 20)) {
        nx2_game_apart_mb = (u32)(kb >> 10), nx2_game_mb = 8992, nx2_game_room_mb = 64;
        nx2_log("[game] the game's own table of heaps keeps %u MB of address space apart from its heap: it is told of the console's 8992 MB, as on its console\\n",
                (u64)nx2_game_apart_mb);
    } else
        nx2_log("[game] the game's own table of heaps keeps no address space apart from its heap\\n");''',
          '''    u32 v = 0, auto_heaps = 0;
    u64 kb = 0, fixed_kb = 0;
    if (byml && size > 16 && byml[0] == 'Y' && byml[1] == 'B') {
        const u64 root = r32(byml + 12);
        if (byml_get(byml, size, root, "Heaps", &v) == 0xC1) byml_scan_heaps(byml, size, v, &kb, &fixed_kb, &auto_heaps);
    }
    if (!byml) nx2_log("[game] the game's table of heaps (%u KB) couldn't be unpacked: it is told of memory as a game that has none\\n", table->size >> 10);
    else if (kb >= 1024 && kb < (16ull << 20)) {
        nx2_game_apart_mb = (u32)(kb >> 10), nx2_game_mb = 8992, nx2_game_room_mb = 64;
        if (fixed_kb > 0 && fixed_kb < (16ull << 20)) nx2_game_fixed_mb = (u32)((fixed_kb + 1023) >> 10);
        nx2_game_auto_heaps = auto_heaps;
        nx2_log("[game] the game's own table of heaps keeps %u MB of address space apart from its heap (%u MB fixed, %u auto): it is told of the console's 8992 MB, as on its console\\n",
                (u64)nx2_game_apart_mb, (u64)nx2_game_fixed_mb, (u64)nx2_game_auto_heaps);
    } else if (fixed_kb >= (5120ull << 10) && fixed_kb < (16ull << 20)) {
        const u32 fixed_mb = (u32)((fixed_kb + 1023) >> 10);
        u32 apart = 8992 > fixed_mb + 900 ? 8992 - fixed_mb - 900 : 1024;
        if (apart < 1024) apart = 1024;
        if (apart > 1500) apart = 1500;
        nx2_game_apart_mb = apart, nx2_game_mb = 8992, nx2_game_room_mb = 64;
        nx2_log("[game] the game's own table of heaps has %u MB of fixed heaps and no VirtualAddressMemory entry: it is told of the console's 8992 MB (assuming %u MB apart)\\n",
                (u64)fixed_mb, (u64)nx2_game_apart_mb);
    } else
        nx2_log("[game] the game's own table of heaps keeps no address space apart from its heap\\n");''')
    patch('main.c',
          '            if (heap_given + apart < most + slack) most = heap_given + apart > slack ? (heap_given + apart - slack) & ~0x1FFFFFull : 0;',
          '''            if (heap_given + apart < most + slack) most = heap_given + apart > slack ? (heap_given + apart - slack) & ~0x1FFFFFull : 0;
#ifdef NX2_LOADER
            if (apart >= (1024ull << 20) && nx2_game_fixed_mb && !nx2_game_auto_heaps && heap_given < most) {
                const u64 min_most = (((u64)nx2_game_fixed_mb + 128) << 20) + apart;
                const u64 want_most = heap_given > slack ? heap_given - slack : 0;
                const u64 fit = ((min_most > want_most ? min_most : want_most) + 0x1FFFFFull) & ~0x1FFFFFull;
                if (fit < most) most = fit;
            }
#endif''')


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
