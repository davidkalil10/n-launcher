// Clean-room IPS and IPS32 binary patcher for Horizon NSO modules.
//
// Applies standard IPS ("PATCH", 24-bit big-endian offsets, "EOF" sentinel) and
// IPS32 ("IPS32", 32-bit big-endian offsets, "EEOF" sentinel) patches to an
// unpacked NSO virtual memory image before it is mapped by the loader service.
//
// Horizon/Atmosphere NSO IPS convention:
//   - Offsets address the uncompressed NSO image including its 0x100-byte NSO0
//     header (patch offset 0x100 maps to virtual offset 0x0, the start of .text).
//   - Hunks targeting the NSO header (< 0x100), alignment gaps, BSS, or crossing
//     a segment boundary are rejected.
//   - Validation runs in a full first pass before any byte is written, so a
//     malformed or out-of-range patch file leaves the module image untouched.
//   - Patch filenames match the NSO's 32-byte Build ID (+0x40 in NSO0 header)
//     in hex (case-insensitive, .ips or .ips32), accepting trailing-zero-trimmed
//     (Atmosphere canonical) as well as 16-, 20-, or full 32-byte forms.
#ifndef NX2_IPS_H
#define NX2_IPS_H

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NX2_IPS_NSO_HEADER_SIZE 0x100u
#define NX2_IPS_BUILD_ID_SIZE   32u
#define NX2_IPS_MAX_FILE_SIZE   (16u * 1024u * 1024u)
#define NX2_IPS_DIR_MAX         256u
#define NX2_IPS_PATH_MAX        768u

#if defined(__GNUC__) || defined(__clang__)
#define NX2_IPS_MAYBE_UNUSED __attribute__((unused))
#else
#define NX2_IPS_MAYBE_UNUSED
#endif

typedef enum {
    NX2_IPS_OK = 0,
    NX2_IPS_ERR_INVALID_ARGS,
    NX2_IPS_ERR_BAD_MAGIC,
    NX2_IPS_ERR_TRUNCATED,
    NX2_IPS_ERR_HEADER_OFFSET,
    NX2_IPS_ERR_OUT_OF_RANGE,
    NX2_IPS_ERR_IO,
    NX2_IPS_ERR_NO_MEMORY
} Nx2IpsStatus;

typedef struct {
    uint64_t offset;
    uint64_t size;
} Nx2IpsSegment;

typedef struct {
    uint32_t hunks;
    uint64_t bytes;
    int is_ips32;
} Nx2IpsResult;

typedef void (*Nx2IpsCallback)(const char *rel_path, Nx2IpsStatus status, const Nx2IpsResult *result, void *user);

NX2_IPS_MAYBE_UNUSED static const char *nx2_ips_status_str(Nx2IpsStatus status) {
    switch (status) {
    case NX2_IPS_OK: return "ok";
    case NX2_IPS_ERR_INVALID_ARGS: return "invalid arguments";
    case NX2_IPS_ERR_BAD_MAGIC: return "bad IPS/IPS32 magic";
    case NX2_IPS_ERR_TRUNCATED: return "truncated patch or missing EOF/EEOF";
    case NX2_IPS_ERR_HEADER_OFFSET: return "offset inside NSO header (< 0x100)";
    case NX2_IPS_ERR_OUT_OF_RANGE: return "hunk outside loaded segment bounds";
    case NX2_IPS_ERR_IO: return "file read error or oversized patch";
    case NX2_IPS_ERR_NO_MEMORY: return "out of memory";
    default: return "unknown error";
    }
}

static int nx2_ips__hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int nx2_ips__lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int nx2_ips__ieq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (nx2_ips__lower((unsigned char)a[i]) != nx2_ips__lower((unsigned char)b[i])) return 0;
    return 1;
}

// Formats the 32-byte NSO Build ID as uppercase hex with trailing 0x00 bytes stripped.
// Returns the written character count (0..64).
NX2_IPS_MAYBE_UNUSED static size_t nx2_ips_format_build_id(const uint8_t build_id[NX2_IPS_BUILD_ID_SIZE], char out[65]) {
    static const char hex[] = "0123456789ABCDEF";
    size_t len = NX2_IPS_BUILD_ID_SIZE;
    while (len > 0 && build_id[len - 1] == 0) len--;
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex[build_id[i] >> 4];
        out[i * 2 + 1] = hex[build_id[i] & 0x0Fu];
    }
    out[len * 2] = '\0';
    return len * 2;
}

// Returns 1 if `filename` is `<hex>.ips` or `<hex>.ips32` (case-insensitive) matching `build_id`.
// Accepts Atmosphere trailing-zero-trimmed IDs as well as zero-padded 16-, 20-, or 32-byte hex IDs.
NX2_IPS_MAYBE_UNUSED static int nx2_ips_filename_matches(const char *filename, const uint8_t build_id[NX2_IPS_BUILD_ID_SIZE]) {
    if (!filename || !filename[0] || filename[0] == '.') return 0;
    size_t trimmed = NX2_IPS_BUILD_ID_SIZE;
    while (trimmed > 0 && build_id[trimmed - 1] == 0) trimmed--;
    if (trimmed == 0) return 0;

    const size_t len = strlen(filename);
    size_t stem_len = 0;
    if (len > 6 && nx2_ips__ieq(filename + len - 6, ".ips32", 6)) stem_len = len - 6;
    else if (len > 4 && nx2_ips__ieq(filename + len - 4, ".ips", 4)) stem_len = len - 4;
    else return 0;

    if (stem_len < 2 || stem_len > NX2_IPS_BUILD_ID_SIZE * 2u || (stem_len & 1u) != 0) return 0;
    const size_t nbytes = stem_len / 2u;
    if (nbytes < trimmed) return 0;

    for (size_t i = 0; i < nbytes; i++) {
        const int hi = nx2_ips__hex_val(filename[i * 2]);
        const int lo = nx2_ips__hex_val(filename[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        const uint8_t byte = (uint8_t)((hi << 4) | lo);
        if (byte != build_id[i]) return 0;
    }
    return 1;
}

// Checks whether [mem_off, mem_off + len) lies completely inside a single loaded segment.
static int nx2_ips__in_segments(uint64_t mem_off, uint64_t len, const Nx2IpsSegment *segs, size_t nsegs) {
    for (size_t s = 0; s < nsegs; s++) {
        if (segs[s].size == 0) continue;
        if (len <= segs[s].size && mem_off >= segs[s].offset && mem_off - segs[s].offset <= segs[s].size - len) return 1;
    }
    return 0;
}

// Validates and applies an IPS or IPS32 patch buffer to an unpacked NSO virtual memory image.
// Zero heap allocations; two-pass atomic (on any error, `image` is not modified).
NX2_IPS_MAYBE_UNUSED static Nx2IpsStatus nx2_ips_apply_buffer(uint8_t *image, uint64_t image_size,
                                                              const Nx2IpsSegment *segs, size_t nsegs,
                                                              const uint8_t *patch, size_t patch_size,
                                                              Nx2IpsResult *out) {
    if (out) {
        out->hunks = 0;
        out->bytes = 0;
        out->is_ips32 = 0;
    }
    if (!image || !segs || nsegs == 0 || !patch) return NX2_IPS_ERR_INVALID_ARGS;
    for (size_t s = 0; s < nsegs; s++) {
        if (segs[s].offset > image_size || segs[s].size > image_size - segs[s].offset) return NX2_IPS_ERR_INVALID_ARGS;
    }
    if (patch_size < 5) return NX2_IPS_ERR_BAD_MAGIC;

    int is_ips32 = 0;
    size_t off_bytes = 3;
    uint32_t eof_marker = 0x454F46u;  // "EOF"
    if (memcmp(patch, "PATCH", 5) == 0) {
        is_ips32 = 0;
    } else if (memcmp(patch, "IPS32", 5) == 0) {
        is_ips32 = 1;
        off_bytes = 4;
        eof_marker = 0x45454F46u;  // "EEOF"
    } else {
        return NX2_IPS_ERR_BAD_MAGIC;
    }

    for (int pass = 0; pass < 2; pass++) {
        size_t pos = 5;
        uint32_t hunks = 0;
        uint64_t bytes = 0;
        int saw_eof = 0;

        while (pos < patch_size) {
            if (patch_size - pos < off_bytes) return NX2_IPS_ERR_TRUNCATED;
            uint32_t raw_off = 0;
            if (is_ips32) {
                raw_off = ((uint32_t)patch[pos] << 24) | ((uint32_t)patch[pos + 1] << 16) |
                          ((uint32_t)patch[pos + 2] << 8) | (uint32_t)patch[pos + 3];
            } else {
                raw_off = ((uint32_t)patch[pos] << 16) | ((uint32_t)patch[pos + 1] << 8) | (uint32_t)patch[pos + 2];
            }
            pos += off_bytes;
            if (raw_off == eof_marker) {
                saw_eof = 1;
                break;
            }
            if (patch_size - pos < 2) return NX2_IPS_ERR_TRUNCATED;
            const uint16_t size = (uint16_t)(((uint16_t)patch[pos] << 8) | (uint16_t)patch[pos + 1]);
            pos += 2;

            if (raw_off < NX2_IPS_NSO_HEADER_SIZE) return NX2_IPS_ERR_HEADER_OFFSET;
            const uint64_t mem_off = (uint64_t)raw_off - NX2_IPS_NSO_HEADER_SIZE;

            if (size > 0) {
                if (patch_size - pos < size) return NX2_IPS_ERR_TRUNCATED;
                if (!nx2_ips__in_segments(mem_off, size, segs, nsegs)) return NX2_IPS_ERR_OUT_OF_RANGE;
                if (pass == 1) memcpy(image + mem_off, patch + pos, size);
                pos += size;
                hunks++;
                bytes += size;
            } else {
                if (patch_size - pos < 3) return NX2_IPS_ERR_TRUNCATED;
                const uint16_t rle_len = (uint16_t)(((uint16_t)patch[pos] << 8) | (uint16_t)patch[pos + 1]);
                const uint8_t rle_val = patch[pos + 2];
                pos += 3;
                if (!nx2_ips__in_segments(mem_off, rle_len, segs, nsegs)) return NX2_IPS_ERR_OUT_OF_RANGE;
                if (pass == 1 && rle_len > 0) memset(image + mem_off, rle_val, rle_len);
                hunks++;
                bytes += rle_len;
            }
        }
        if (!saw_eof) return NX2_IPS_ERR_TRUNCATED;
        if (pass == 1 && out) {
            out->hunks = hunks;
            out->bytes = bytes;
            out->is_ips32 = is_ips32;
        }
    }
    return NX2_IPS_OK;
}

typedef struct {
    char name[256];
} Nx2IpsName;

static int nx2_ips__cmp_name(const void *a, const void *b) {
    return strcmp(((const Nx2IpsName *)a)->name, ((const Nx2IpsName *)b)->name);
}

static int nx2_ips__join(char *out, size_t cap, const char *a, const char *b) {
    const size_t la = strlen(a), lb = strlen(b);
    const size_t slash = (la > 0 && a[la - 1] != '/') ? 1u : 0u;
    if (la + slash + lb + 1u > cap) return 0;
    memcpy(out, a, la);
    if (slash) out[la] = '/';
    memcpy(out + la + slash, b, lb + 1u);
    return 1;
}

static int nx2_ips__push_name(Nx2IpsName **list, size_t *count, size_t *cap, const char *name) {
    const size_t len = strlen(name);
    if (len == 0 || len >= sizeof((*list)[0].name) || *count >= NX2_IPS_DIR_MAX) return 0;
    if (*count == *cap) {
        const size_t next = *cap ? (*cap * 2u) : 16u;
        Nx2IpsName *grown = (Nx2IpsName *)realloc(*list, next * sizeof(Nx2IpsName));
        if (!grown) return 0;
        *list = grown;
        *cap = next;
    }
    memcpy((*list)[*count].name, name, len + 1u);
    (*count)++;
    return 1;
}

// Reads a patch file from disk and applies it to `image`.
NX2_IPS_MAYBE_UNUSED static Nx2IpsStatus nx2_ips_apply_file(const char *path,
                                                            uint8_t *image, uint64_t image_size,
                                                            const Nx2IpsSegment *segs, size_t nsegs,
                                                            Nx2IpsResult *out) {
    if (out) {
        out->hunks = 0;
        out->bytes = 0;
        out->is_ips32 = 0;
    }
    if (!path) return NX2_IPS_ERR_INVALID_ARGS;
    FILE *f = fopen(path, "rb");
    if (!f) return NX2_IPS_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NX2_IPS_ERR_IO;
    }
    const long sz = ftell(f);
    if (sz < 0 || (uint64_t)sz > NX2_IPS_MAX_FILE_SIZE || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NX2_IPS_ERR_IO;
    }
    if (sz == 0) {
        fclose(f);
        return NX2_IPS_ERR_BAD_MAGIC;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NX2_IPS_ERR_NO_MEMORY;
    }
    const size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        return NX2_IPS_ERR_IO;
    }
    const Nx2IpsStatus st = nx2_ips_apply_buffer(image, image_size, segs, nsegs, buf, got, out);
    free(buf);
    return st;
}

// Scans `patches_dir` for matching `<BUILD_ID>.ips[32]` files directly inside it and
// one directory level below (`<patches_dir>/<mod>/<BUILD_ID>.ips[32]`), applying them
// in deterministic alphabetical order (direct files first, then subdirectories).
// Returns the number of patch files successfully applied.
NX2_IPS_MAYBE_UNUSED static unsigned nx2_ips_apply_dir(const char *patches_dir,
                                                       const uint8_t build_id[NX2_IPS_BUILD_ID_SIZE],
                                                       uint8_t *image, uint64_t image_size,
                                                       const Nx2IpsSegment *segs, size_t nsegs,
                                                       Nx2IpsCallback cb, void *user) {
    if (!patches_dir || !patches_dir[0]) return 0;
    DIR *dir = opendir(patches_dir);
    if (!dir) return 0;

    Nx2IpsName *files = NULL, *subdirs = NULL;
    size_t nfiles = 0, cfiles = 0, nsubdirs = 0, csubdirs = 0;
    char full[NX2_IPS_PATH_MAX];

    for (struct dirent *e; (e = readdir(dir)) != NULL;) {
        if (e->d_name[0] == '.') continue;
        if (!nx2_ips__join(full, sizeof(full), patches_dir, e->d_name)) continue;
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            nx2_ips__push_name(&subdirs, &nsubdirs, &csubdirs, e->d_name);
        } else if (S_ISREG(st.st_mode) && nx2_ips_filename_matches(e->d_name, build_id)) {
            nx2_ips__push_name(&files, &nfiles, &cfiles, e->d_name);
        }
    }
    closedir(dir);

    if (nfiles > 1) qsort(files, nfiles, sizeof(Nx2IpsName), nx2_ips__cmp_name);
    if (nsubdirs > 1) qsort(subdirs, nsubdirs, sizeof(Nx2IpsName), nx2_ips__cmp_name);

    unsigned applied = 0;
    for (size_t i = 0; i < nfiles; i++) {
        if (!nx2_ips__join(full, sizeof(full), patches_dir, files[i].name)) continue;
        Nx2IpsResult res;
        const Nx2IpsStatus st = nx2_ips_apply_file(full, image, image_size, segs, nsegs, &res);
        if (st == NX2_IPS_OK) applied++;
        if (cb) cb(files[i].name, st, &res, user);
    }

    char subdir_path[NX2_IPS_PATH_MAX];
    char rel[NX2_IPS_PATH_MAX];
    for (size_t d = 0; d < nsubdirs; d++) {
        if (!nx2_ips__join(subdir_path, sizeof(subdir_path), patches_dir, subdirs[d].name)) continue;
        DIR *sub = opendir(subdir_path);
        if (!sub) continue;
        Nx2IpsName *subfiles = NULL;
        size_t nsubfiles = 0, csubfiles = 0;
        for (struct dirent *e; (e = readdir(sub)) != NULL;) {
            if (e->d_name[0] == '.') continue;
            if (!nx2_ips_filename_matches(e->d_name, build_id)) continue;
            if (!nx2_ips__join(full, sizeof(full), subdir_path, e->d_name)) continue;
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            nx2_ips__push_name(&subfiles, &nsubfiles, &csubfiles, e->d_name);
        }
        closedir(sub);
        if (nsubfiles > 1) qsort(subfiles, nsubfiles, sizeof(Nx2IpsName), nx2_ips__cmp_name);
        for (size_t i = 0; i < nsubfiles; i++) {
            if (!nx2_ips__join(full, sizeof(full), subdir_path, subfiles[i].name)) continue;
            if (!nx2_ips__join(rel, sizeof(rel), subdirs[d].name, subfiles[i].name)) continue;
            Nx2IpsResult res;
            const Nx2IpsStatus st = nx2_ips_apply_file(full, image, image_size, segs, nsegs, &res);
            if (st == NX2_IPS_OK) applied++;
            if (cb) cb(rel, st, &res, user);
        }
        free(subfiles);
    }

    free(files);
    free(subdirs);
    return applied;
}

#endif
