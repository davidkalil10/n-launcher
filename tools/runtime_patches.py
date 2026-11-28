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
    # Fix for Red Dead Redemption (0x35f202 ResultMountNameNotFound):
    # RDR calls nn::fs::GetEntryType("save:/\\gabindings.xml") immediately after Unmount("save") when OpenFile returns 0x202.
    # Track the active save mount and temporarily remount around GetEntryType if the game queries the unmounted save prefix.
    # Also keeps opt-in <folder>/fstrace diagnostics for nn::fs and nn::ro::LoadModule.
    patch('gl2.c',
'''#ifdef NX2_GAME_MB
static u32 (*real_get_save_data_size)(s64 *, s64 *, const void *);
static u32 get_save_data_size_watched(s64 *out_data, s64 *out_journal, const void *uid) {
    return nx2_get_save_data_size(out_data, out_journal, uid, real_get_save_data_size);
}
#endif''',
'''#ifdef NX2_GAME_MB
static u32 (*real_get_save_data_size)(s64 *, s64 *, const void *);
static u32 get_save_data_size_watched(s64 *out_data, s64 *out_journal, const void *uid) {
    return nx2_get_save_data_size(out_data, out_journal, uid, real_get_save_data_size);
}
static int fstrace_on;
static char fstrace_rom[32];
static u32 fstrace_rom_hits;
static int fstrace_budget = 400;
static char nl_save_mount[32];
static u8 nl_save_uid[16];
static int nl_save_mounted;
static u32 nl_save_lock;
static void nl_lock(u32 *lk) { while (__atomic_exchange_n(lk, 1, __ATOMIC_ACQUIRE)) svcSleepThread(10000); }
static void nl_unlock(u32 *lk) { __atomic_store_n(lk, 0, __ATOMIC_RELEASE); }
static int nl_has_prefix(const char *path, const char *mount) {
    if (!path || !mount || !mount[0]) return 0;
    u32 k = 0;
    while (mount[k] && path[k] == mount[k]) k++;
    return !mount[k] && path[k] == ':';
}
static u32 (*real_mount_rom)(const char *, void *, u64);
static u32 mount_rom_traced(const char *name, void *cache, u64 size) {
    if (name) {
        u32 k = 0;
        for (; k + 1 < sizeof fstrace_rom && name[k]; k++) fstrace_rom[k] = name[k];
        fstrace_rom[k] = 0;
    }
    if (fstrace_on) nx2_log("[fstrace] MountRom('%s', size=%x)\\n", name ? name : "(null)", size);
    const u32 rc = real_mount_rom(name, cache, size);
    if (fstrace_on) nx2_log("[fstrace] MountRom('%s') -> %x\\n", name ? name : "(null)", (u64)rc);
    return rc;
}
static u32 (*real_mount_save)(const char *, const void *);
static u32 mount_save_watched(const char *name, const void *uid) {
    if (fstrace_on) nx2_log("[fstrace] MountSaveData('%s')\\n", name ? name : "(null)");
    const u32 rc = real_mount_save(name, uid);
    if (rc == 0 && name && uid) {
        nl_lock(&nl_save_lock);
        u32 k = 0;
        for (; k + 1 < sizeof nl_save_mount && name[k]; k++) nl_save_mount[k] = name[k];
        nl_save_mount[k] = 0;
        for (u32 b = 0; b < 16; b++) nl_save_uid[b] = ((const u8 *)uid)[b];
        nl_save_mounted = 1;
        nl_unlock(&nl_save_lock);
    }
    if (fstrace_on) nx2_log("[fstrace] MountSaveData('%s') -> %x\\n", name ? name : "(null)", (u64)rc);
    return rc;
}
static void (*real_unmount)(const char *);
static void unmount_watched(const char *name) {
    if (fstrace_on) nx2_log("[fstrace] Unmount('%s')\\n", name ? name : "(null)");
    if (name) {
        nl_lock(&nl_save_lock);
        if (nl_save_mounted && !strcmp(name, nl_save_mount)) nl_save_mounted = 0;
        nl_unlock(&nl_save_lock);
    }
    real_unmount(name);
}
static u32 (*real_get_entry_type)(void *, const char *);
static u32 get_entry_type_watched(void *out, const char *path) {
    char clean[512];
    const char *use = path;
    if (path) {
        int has_bs = 0;
        for (u32 k = 0; path[k]; k++)
            if (path[k] == '\\\\') { has_bs = 1; break; }
        if (has_bs) {
            u32 n = 0;
            for (u32 k = 0; path[k] && n + 1 < sizeof clean; k++) {
                char c = path[k] == '\\\\' ? '/' : path[k];
                if (c == '/' && n > 0 && clean[n - 1] == '/') continue;
                clean[n++] = c;
            }
            clean[n] = 0;
            use = clean;
            static u32 bs_logged;
            if (bs_logged++ < 8) nx2_log("[save] GetEntryType sanitized '%s' -> '%s'\\n", path, clean);
        }
    }
    const int is_rom = nl_has_prefix(use, fstrace_rom);
    const int say = fstrace_on && (!is_rom || __atomic_fetch_add(&fstrace_rom_hits, 1, __ATOMIC_RELAXED) < 5) &&
                    __atomic_fetch_sub(&fstrace_budget, 1, __ATOMIC_RELAXED) > 0;
    if (say) nx2_log("[fstrace] GetEntryType('%s')\\n", use ? use : "(null)");
    if (real_mount_save && real_unmount && nl_has_prefix(use, nl_save_mount)) {
        nl_lock(&nl_save_lock);
        const int need_remount = !nl_save_mounted && nl_save_mount[0] != 0;
        if (need_remount) {
            static u32 remount_logged;
            if (remount_logged++ < 8)
                nx2_log("[save] GetEntryType('%s') called while '%s' is unmounted: remounting temporarily\\n", use, nl_save_mount);
            const u32 mrc = real_mount_save(nl_save_mount, nl_save_uid);
            const u32 rc = real_get_entry_type(out, use);
            if (mrc == 0) real_unmount(nl_save_mount);
            nl_unlock(&nl_save_lock);
            if (say) nx2_log("[fstrace] GetEntryType('%s') [remounted] -> %x\\n", use, (u64)rc);
            return rc;
        }
        nl_unlock(&nl_save_lock);
    }
    const u32 rc = real_get_entry_type(out, use);
    if (say) nx2_log("[fstrace] GetEntryType('%s') -> %x\\n", use ? use : "(null)", (u64)rc);
    return rc;
}
static u32 (*real_open_file)(void *, const char *, int);
static u32 open_file_traced(void *out, const char *path, int mode) {
    const int is_rom = nl_has_prefix(path, fstrace_rom);
    const int say = fstrace_on && !is_rom && __atomic_fetch_sub(&fstrace_budget, 1, __ATOMIC_RELAXED) > 0;
    if (say) nx2_log("[fstrace] OpenFile('%s', mode=%x)\\n", path ? path : "(null)", (u64)(u32)mode);
    const u32 rc = real_open_file(out, path, mode);
    if (say) nx2_log("[fstrace] OpenFile('%s') -> %x\\n", path ? path : "(null)", (u64)rc);
    return rc;
}
static u32 (*real_ro_load_module)(void *, const void *, void *, u64, int);
static u32 ro_load_module_traced(void *mod, const void *img, void *buf, u64 size, int flag) {
    if (fstrace_on) nx2_log("[fstrace] ro::LoadModule(img=%p, buf=%p, size=%x, flag=%x)\\n", (u64)img, (u64)buf, size, (u64)(u32)flag);
    const u32 rc = real_ro_load_module(mod, img, buf, size, flag);
    if (fstrace_on) nx2_log("[fstrace] ro::LoadModule -> %x\\n", (u64)rc);
    return rc;
}
#endif''')
    patch('gl2.c',
'''        if (!strcmp(name, "_ZN2nn2fs15GetSaveDataSizeEPlS1_RKNS_7account3UidE")) {
            real_get_save_data_size = (u32 (*)(s64 *, s64 *, const void *))nx2_symbol(name);
            if (real_get_save_data_size) return get_save_data_size_watched;
        }''',
'''        if (!strcmp(name, "_ZN2nn2fs15GetSaveDataSizeEPlS1_RKNS_7account3UidE")) {
            real_get_save_data_size = (u32 (*)(s64 *, s64 *, const void *))nx2_symbol(name);
            if (real_get_save_data_size) return get_save_data_size_watched;
        }
        static int fstrace = -1;
        if (fstrace < 0) {
            FILE *f = fopen(NX2_F("fstrace"), "rb");
            fstrace = f != NULL;
            if (f) fclose(f);
            fstrace_on = fstrace;
            if (fstrace) nx2_log("[fstrace] <folder>/fstrace is there: logging nn::fs mounts/paths and nn::ro::LoadModule\\n");
        }
        if (!strcmp(name, "_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE")) {
            real_mount_save = (u32 (*)(const char *, const void *))nx2_symbol(name);
            if (real_mount_save) return mount_save_watched;
        }
        if (!strcmp(name, "_ZN2nn2fs7UnmountEPKc")) {
            real_unmount = (void (*)(const char *))nx2_symbol(name);
            if (real_unmount) return unmount_watched;
        }
        if (!strcmp(name, "_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc")) {
            real_get_entry_type = (u32 (*)(void *, const char *))nx2_symbol(name);
            if (real_get_entry_type) return get_entry_type_watched;
        }
        if (fstrace) {
            if (!strcmp(name, "_ZN2nn2fs8MountRomEPKcPvm")) {
                real_mount_rom = (u32 (*)(const char *, void *, u64))nx2_symbol(name);
                if (real_mount_rom) return mount_rom_traced;
            }
            if (!strcmp(name, "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci")) {
                real_open_file = (u32 (*)(void *, const char *, int))nx2_symbol(name);
                if (real_open_file) return open_file_traced;
            }
            if (!strcmp(name, "_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi")) {
                real_ro_load_module = (u32 (*)(void *, const void *, void *, u64, int))nx2_symbol(name);
                if (real_ro_load_module) return ro_load_module_traced;
            }
        }''')
    # Log exact caller site and manual_lookup pointer on unresolved PLT symbol (without unsafe frame walk on PLT stack).
    patch('loader.c',
'''        } else {
            nx2_log("[load] the game called %s, which no module defines" "\\n", plt[i].name);
            nx2_crash("the game called a function that none of its modules has (the log's last \\"[load]\\" line names it)");
            nx2_exit();
        }''',
'''        } else {
            const char *cm = "?";
            u64 co = from;
            nx2_where(from, &cm, &co);
            nx2_log("[load] the game called %s from %s+%x (manual_lookup=%p), which no module defines" "\\n", plt[i].name, cm, co - 4, (u64)manual_lookup);
            nx2_crash("the game called a function that none of its modules has (the log's last \\"[load]\\" line names it)");
            nx2_exit();
        }''')
    # Both fallback and normal return paths point to our app, without changing activation.
    for p in (out/'loader/runtime').glob('*.c'):
        s=p.read_text(encoding='utf-8');s=s.replace('sdmc:/switch/nx2-flow/nx2-flow.nro','sdmc:/switch/n-launcher/n-launcher.nro').replace('sdmc:/nx2-flow/next.txt','sdmc:/switch/n-launcher/next.txt')
        p.write_text(s,encoding='utf-8',newline='\n')
