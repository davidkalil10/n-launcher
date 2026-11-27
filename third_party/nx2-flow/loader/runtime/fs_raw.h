// Raw CMIF IPC helpers for fsp-srv (IFile / IFileSystem) safe on game threads.
//
// libounce's fsFileRead / fsFsOpenFile / fsFileClose / fsFsGetEntryType / fsFsCreateDirectory
// route calls through g_fsSessionMgr (sessionmgrAttachClient -> mutexLock) and read the
// thread-local g_fsPriority via __aarch64_read_tp(). Game threads created by nnSdk do not have
// libounce ThreadVars in TLS (tpidrro_el0 + 0x1E0..0x1FF): under contention mutexLock passes an
// invalid thread handle to svcArbitrateLock and calls svcBreak(1, 0, 0) (2168-0006), while
// __aarch64_read_tp() dereferences nnSdk's ThreadType pointer as a libounce TLS block.
//
// These helpers talk directly to f->s.session / fs->s.session with raw HIPC/CMIF messages in the
// caller's kernel TLS IPC buffer (armGetTls(), saved and restored around each call) and serialize
// concurrent callers with an atomic spinlock using only svcSleepThread.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "nx2.h"

#ifndef NX2_FS_RAW_TEST
#include <switch.h>
static inline u8 *nx2_fs_tls(void) { return (u8 *)armGetTls(); }
static inline u32 nx2_fs_send_sync(u32 handle) { return (u32)svcSendSyncRequest(handle); }
static inline void nx2_fs_close_handle(u32 handle) { (void)svcCloseHandle(handle); }
static inline void nx2_fs_sleep(s64 ns) { (void)svcSleepThread(ns); }
#else
u8 *nx2_fs_tls(void);
u32 nx2_fs_send_sync(u32 handle);
void nx2_fs_close_handle(u32 handle);
void nx2_fs_sleep(s64 ns);
#endif

// Atomic spinlock safe on any thread (no libounce Mutex / ThreadVars).
// Spins briefly with yield (0 ns), then sleeps 100 us so lower-priority threads (e.g. unpacker)
// cannot be starved by higher-priority game threads waiting for an SD read to finish.
static inline void nx2_fs_lock(u32 *lock) {
    u32 spins = 0;
    while (__atomic_exchange_n(lock, 1u, __ATOMIC_ACQUIRE)) {
        nx2_fs_sleep(spins < 16u ? 0 : 100000);
        if (spins < 16u) spins++;
    }
}

static inline void nx2_fs_unlock(u32 *lock) {
    __atomic_store_n(lock, 0u, __ATOMIC_RELEASE);
}

static inline u32 nx2_fs_r32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline u64 nx2_fs_r64(const u8 *p) {
    return (u64)nx2_fs_r32(p) | ((u64)nx2_fs_r32(p + 4) << 32);
}

static inline void nx2_fs_w32(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static inline void nx2_fs_w64(u8 *p, u64 v) {
    nx2_fs_w32(p, (u32)v);
    nx2_fs_w32(p + 4, (u32)(v >> 32));
}

static inline void nx2_fs_zero(u8 *p, u32 n) {
    for (u32 k = 0; k < n; k++) p[k] = 0;
}

static inline void nx2_fs_copy(u8 *dst, const u8 *src, u32 n) {
    for (u32 k = 0; k < n; k++) dst[k] = src[k];
}

// Parse a CMIF response in `m` (0x100 bytes).
// On success returns 0 and optionally sets *out_data_off (offset in `m` of CMIF out payload)
// and *out_obj (domain object_id or non-domain move handle).
static inline u32 nx2_fs_parse_response(const u8 *m, int is_domain, u32 out_data_size,
                                        int want_object, u32 *out_data_off, u32 *out_obj) {
    const u32 w0 = nx2_fs_r32(m), w1 = nx2_fs_r32(m + 4);
    const u32 num_statics = (w0 >> 16) & 0xFu;
    const u32 has_special = (w1 >> 31) & 1u;
    u32 off = 8u, move_off = 0u, num_move = 0u;
    if (has_special) {
        const u32 sphdr = nx2_fs_r32(m + off);
        off += 4u;
        if (sphdr & 1u) off += 8u;
        const u32 num_copy = (sphdr >> 1) & 0xFu;
        num_move = (sphdr >> 5) & 0xFu;
        off += num_copy * 4u;
        move_off = off;
        off += num_move * 4u;
    }
    off += num_statics * 8u;
    off = (off + 15u) & ~15u;
    const u32 cmif_off = is_domain ? off + 16u : off;
    const u32 need = cmif_off + 16u + out_data_size + (is_domain && want_object ? 4u : 0u);
    if (need > 0x100u) return 0x5F59u;  // LibnxError_InvalidCmifOutHeader
    if (nx2_fs_r32(m + cmif_off) != 0x4F434653u) return 0x5F59u;  // "SFCO"
    const u32 res = nx2_fs_r32(m + cmif_off + 8u);
    if (res) return res;
    if (out_data_off) *out_data_off = cmif_off + 16u;
    if (want_object) {
        if (is_domain) {
            if (out_obj) *out_obj = nx2_fs_r32(m + cmif_off + 16u + out_data_size);
        } else {
            if (num_move < 1u || move_off + 4u > 0x100u) return 0x5F59u;
            if (out_obj) *out_obj = nx2_fs_r32(m + move_off);
        }
    }
    return 0;
}

// Copy a NUL-terminated path into a 0x301-byte (FS_MAX_PATH) zero-padded buffer without libc.
static inline int nx2_fs_copy_path(char dst[0x301], const char *src) {
    if (!src) return 0;
    u32 n = 0;
    while (src[n]) {
        if (n >= 0x300u) return 0;
        dst[n] = src[n];
        n++;
    }
    while (n < 0x301u) dst[n++] = 0;
    return 1;
}

// Raw CMIF IFile::Read (cmd 0) using HipcMapAlias | Out | NonSecure (mode 1).
static inline u32 nx2_fs_file_read(const FsFile *f, s64 off, void *buf, u64 size, u64 *bytes_read) {
    if (bytes_read) *bytes_read = 0;
    if (!f || !f->s.session || off < 0) return 0x2EE202u;
    if (!size) return 0;
    if (!buf) return 0x2EE202u;

    u8 *m = nx2_fs_tls();
    u8 saved[0x100];
    nx2_fs_copy(saved, m, 0x100u);
    nx2_fs_zero(m, 0x100u);

    const u32 obj = f->s.object_id;
    nx2_fs_w32(m + 0, 4u | (1u << 24));          // CmifCommandType_Request, 1 recv buffer
    nx2_fs_w32(m + 4, obj ? 18u : 14u);          // data words (16 pad + [16 domain] + 16 SFCI + 24 payload)

    // HipcBufferDescriptor (recv buffer 0): HipcBufferMode_NonSecure = 1 (matches libounce fsFileRead)
    const u64 addr = (u64)(uintptr_t)buf;
    nx2_fs_w32(m + 8, (u32)size);
    nx2_fs_w32(m + 12, (u32)addr);
    const u32 flags = 1u |
                      ((u32)((addr >> 36) & 0x3FFFFFu) << 2) |
                      ((u32)((size >> 32) & 0xFu) << 24) |
                      ((u32)((addr >> 32) & 0xFu) << 28);
    nx2_fs_w32(m + 16, flags);

    u8 *q = m + 0x20;
    if (obj) {
        m[0x20] = 1u;                            // CmifDomainRequestType_SendMessage
        m[0x22] = 0x28u;                         // payload size = 16 (SFCI) + 24 (args) = 40
        nx2_fs_w32(m + 0x24, obj);
        q = m + 0x30;
    }
    nx2_fs_w32(q + 0, 0x49434653u);              // "SFCI"
    nx2_fs_w32(q + 8, 0u);                       // IFile::Read (cmd 0)
    nx2_fs_w32(q + 0x10, 0u);                    // FsReadOption_None
    nx2_fs_w32(q + 0x14, 0u);
    nx2_fs_w64(q + 0x18, (u64)off);
    nx2_fs_w64(q + 0x20, size);

    u32 rc = nx2_fs_send_sync(f->s.session);
    if (!rc) {
        u32 out_off = 0;
        rc = nx2_fs_parse_response(m, obj != 0, 8u, 0, &out_off, NULL);
        if (!rc && bytes_read) *bytes_read = nx2_fs_r64(m + out_off);
    }

    nx2_fs_copy(m, saved, 0x100u);
    return rc;
}

// Raw CMIF IFileSystem::OpenFile (cmd 8).
static inline u32 nx2_fs_open_file(const FsFileSystem *fs, const char *path, u32 mode, FsFile *out) {
    if (out) {
        out->s.session = 0;
        out->s.own_handle = 0;
        out->s.object_id = 0;
        out->s.pointer_buffer_size = 0;
    }
    if (!fs || !fs->s.session || !path || !out) return 0x2EE202u;

    char pbuf[0x301];
    if (!nx2_fs_copy_path(pbuf, path)) return 0x2EE202u;

    u8 *m = nx2_fs_tls();
    u8 saved[0x100];
    nx2_fs_copy(saved, m, 0x100u);
    nx2_fs_zero(m, 0x100u);

    const u32 obj = fs->s.object_id;
    nx2_fs_w32(m + 0, 4u | (1u << 16));          // CmifCommandType_Request, 1 send static
    nx2_fs_w32(m + 4, obj ? 13u : 9u);           // data words (16 pad + [16 domain] + 16 SFCI + 4 mode)

    const u64 p = (u64)(uintptr_t)pbuf;
    nx2_fs_w32(m + 8, (0x301u << 16) | ((u32)((p >> 32) & 0xFu) << 12) | ((u32)((p >> 36) & 0x3Fu) << 6));
    nx2_fs_w32(m + 12, (u32)p);

    u8 *q = m + 0x10;
    if (obj) {
        m[0x10] = 1u;                            // CmifDomainRequestType_SendMessage
        m[0x12] = 0x14u;                         // payload size = 16 (SFCI) + 4 (mode) = 20
        nx2_fs_w32(m + 0x14, obj);
        q = m + 0x20;
    }
    nx2_fs_w32(q + 0, 0x49434653u);              // "SFCI"
    nx2_fs_w32(q + 8, 8u);                       // IFileSystem::OpenFile (cmd 8)
    nx2_fs_w32(q + 0x10, mode);

    u32 rc = nx2_fs_send_sync(fs->s.session);
    if (!rc) {
        u32 out_id = 0;
        rc = nx2_fs_parse_response(m, obj != 0, 0u, 1, NULL, &out_id);
        if (!rc) {
            if (!out_id) {
                rc = 0x5F59u;
            } else if (obj) {
                out->s.session = fs->s.session;
                out->s.own_handle = 0;
                out->s.object_id = out_id;
                out->s.pointer_buffer_size = fs->s.pointer_buffer_size;
            } else {
                out->s.session = out_id;
                out->s.own_handle = 1;
                out->s.object_id = 0;
                out->s.pointer_buffer_size = fs->s.pointer_buffer_size;
            }
        }
    }

    nx2_fs_copy(m, saved, 0x100u);
    return rc;
}

// Raw CMIF close for an FsFile (domain object close or handle close).
static inline void nx2_fs_file_close(FsFile *f) {
    if (!f || !f->s.session) return;
    u8 *m = nx2_fs_tls();
    u8 saved[0x100];
    nx2_fs_copy(saved, m, 0x100u);
    nx2_fs_zero(m, 0x100u);

    if (f->s.own_handle) {
        nx2_fs_w32(m + 0, 2u);                   // CmifCommandType_Close
        (void)nx2_fs_send_sync(f->s.session);
        nx2_fs_close_handle(f->s.session);
    } else if (f->s.object_id) {
        nx2_fs_w32(m + 0, 4u);                   // CmifCommandType_Request
        nx2_fs_w32(m + 4, 8u);
        m[0x10] = 2u;                            // CmifDomainRequestType_Close
        nx2_fs_w32(m + 0x14, f->s.object_id);
        (void)nx2_fs_send_sync(f->s.session);
    }

    nx2_fs_copy(m, saved, 0x100u);
    f->s.session = 0;
    f->s.own_handle = 0;
    f->s.object_id = 0;
    f->s.pointer_buffer_size = 0;
}

// Raw CMIF IFileSystem::GetEntryType (cmd 7).
static inline u32 nx2_fs_get_entry_type(const FsFileSystem *fs, const char *path, FsDirEntryType *out_type) {
    if (!fs || !fs->s.session || !path || !out_type) return 0x2EE202u;
    char pbuf[0x301];
    if (!nx2_fs_copy_path(pbuf, path)) return 0x2EE202u;

    u8 *m = nx2_fs_tls();
    u8 saved[0x100];
    nx2_fs_copy(saved, m, 0x100u);
    nx2_fs_zero(m, 0x100u);

    const u32 obj = fs->s.object_id;
    nx2_fs_w32(m + 0, 4u | (1u << 16));
    nx2_fs_w32(m + 4, obj ? 12u : 8u);

    const u64 p = (u64)(uintptr_t)pbuf;
    nx2_fs_w32(m + 8, (0x301u << 16) | ((u32)((p >> 32) & 0xFu) << 12) | ((u32)((p >> 36) & 0x3Fu) << 6));
    nx2_fs_w32(m + 12, (u32)p);

    u8 *q = m + 0x10;
    if (obj) {
        m[0x10] = 1u;
        m[0x12] = 0x10u;
        nx2_fs_w32(m + 0x14, obj);
        q = m + 0x20;
    }
    nx2_fs_w32(q + 0, 0x49434653u);
    nx2_fs_w32(q + 8, 7u);                       // IFileSystem::GetEntryType (cmd 7)

    u32 rc = nx2_fs_send_sync(fs->s.session);
    if (!rc) {
        u32 out_off = 0;
        rc = nx2_fs_parse_response(m, obj != 0, 4u, 0, &out_off, NULL);
        if (!rc) *out_type = (FsDirEntryType)nx2_fs_r32(m + out_off);
    }

    nx2_fs_copy(m, saved, 0x100u);
    return rc;
}

// Raw CMIF IFileSystem::CreateDirectory (cmd 2).
static inline u32 nx2_fs_create_directory(const FsFileSystem *fs, const char *path) {
    if (!fs || !fs->s.session || !path) return 0x2EE202u;
    char pbuf[0x301];
    if (!nx2_fs_copy_path(pbuf, path)) return 0x2EE202u;

    u8 *m = nx2_fs_tls();
    u8 saved[0x100];
    nx2_fs_copy(saved, m, 0x100u);
    nx2_fs_zero(m, 0x100u);

    const u32 obj = fs->s.object_id;
    nx2_fs_w32(m + 0, 4u | (1u << 16));
    nx2_fs_w32(m + 4, obj ? 12u : 8u);

    const u64 p = (u64)(uintptr_t)pbuf;
    nx2_fs_w32(m + 8, (0x301u << 16) | ((u32)((p >> 32) & 0xFu) << 12) | ((u32)((p >> 36) & 0x3Fu) << 6));
    nx2_fs_w32(m + 12, (u32)p);

    u8 *q = m + 0x10;
    if (obj) {
        m[0x10] = 1u;
        m[0x12] = 0x10u;
        nx2_fs_w32(m + 0x14, obj);
        q = m + 0x20;
    }
    nx2_fs_w32(q + 0, 0x49434653u);
    nx2_fs_w32(q + 8, 2u);                       // IFileSystem::CreateDirectory (cmd 2)

    u32 rc = nx2_fs_send_sync(fs->s.session);
    if (!rc) rc = nx2_fs_parse_response(m, obj != 0, 0u, 0, NULL, NULL);

    nx2_fs_copy(m, saved, 0x100u);
    return rc;
}

typedef struct {
    FsFile file;
    u64 size;
} Nx2FsPart;

// Read `size` bytes at virtual offset `at` across a multi-part file (4 MiB max per IPC chunk),
// serialized per chunk with `lock` when non-NULL.
static inline u32 nx2_fs_read_parts(const Nx2FsPart *parts, u32 nparts, u32 *lock, u64 at, void *to, u64 size) {
    u8 *out = (u8 *)to;
    u32 k = 0;
    u64 in = at;
    while (k < nparts && in >= parts[k].size) in -= parts[k++].size;
    while (size) {
        if (k >= nparts) return 0x2EE202u;
        const u64 left = parts[k].size - in;
        u64 take = size < (4u << 20) ? size : (4u << 20), got = 0;
        if (take > left) take = left;
        if (lock) nx2_fs_lock(lock);
        const u32 rc = nx2_fs_file_read(&parts[k].file, (s64)in, out, take, &got);
        if (lock) nx2_fs_unlock(lock);
        if (rc) return rc;
        if (got != take) return 0x2EE202u;
        in += take;
        out += take;
        size -= take;
        if (in == parts[k].size) k++, in = 0;
    }
    return 0;
}
