// Fallback for appletOE IApplicationFunctions::PopLaunchParameter(PreselectedUser).
// Runs on the game's threads: no C library, no TLS, lock-free atomics only.
#include "preselected.h"

#include <stddef.h>

#define PRESEL_SLOTS         8u
#define PRESEL_STORAGE_BASE  0x41435300u  // "ACS\0" + slot (virtual IStorage)
#define PRESEL_ACCESSOR_BASE 0x41434100u  // "ACA\0" + slot (virtual IStorageAccessor)

static uint64_t g_uid_lo;
static uint64_t g_uid_hi;
static int g_uid_valid;

static uint32_t g_tracked_proxy;
static uint32_t g_app_funcs;
static uint32_t g_storage_mask;
static uint32_t g_accessor_mask;

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}
static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void wr64(uint8_t *p, uint64_t v) {
    wr32(p, (uint32_t)v);
    wr32(p + 4, (uint32_t)(v >> 32));
}

// Parse a 0x100-byte HIPC message buffer: returns the 16-byte aligned start of CMIF
// domain payload, and optionally extracts the first receive buffer (B-buffer or
// HipcAutoSelect C-buffer / HipcRecvListEntry).
static const uint8_t *ipc_parse(const uint8_t *m, uint64_t *out_buf, uint64_t *out_size) {
    const uint32_t w0 = rd32(m);
    const uint32_t w1 = rd32(m + 4);
    uint32_t o = 8u;
    if (w1 >> 31) {
        if (o + 4u > 0x100u) return NULL;
        const uint32_t s = rd32(m + o);
        o += 4u + ((s & 1u) ? 8u : 0u) + (((s >> 1) & 15u) + ((s >> 5) & 15u)) * 4u;
    }
    const uint32_t n_send_s = (w0 >> 16) & 15u;
    const uint32_t n_send_b = (w0 >> 20) & 15u;
    const uint32_t n_recv_b = (w0 >> 24) & 15u;
    const uint32_t n_exch_b = (w0 >> 28) & 15u;
    o += n_send_s * 8u + n_send_b * 12u;

    uint64_t b_addr = 0, b_size = 0;
    if (n_recv_b && o + 12u <= 0x100u) {
        const uint32_t size_lo = rd32(m + o);
        const uint32_t addr_lo = rd32(m + o + 4u);
        const uint32_t hi = rd32(m + o + 8u);
        b_addr = (uint64_t)addr_lo | ((uint64_t)((hi >> 28) & 15u) << 32) | ((uint64_t)((hi >> 2) & 0x3FFFFFu) << 36);
        b_size = (uint64_t)size_lo | ((uint64_t)((hi >> 24) & 15u) << 32);
    }
    o += (n_recv_b + n_exch_b) * 12u;

    const uint32_t data_words_off = o;
    const uint32_t aligned = (o + 15u) & ~15u;
    if (aligned > 0x100u) return NULL;

    if (out_buf && out_size) {
        *out_buf = b_addr;
        *out_size = b_size;
        if (!b_addr && !b_size) {
            const uint32_t recv_mode = (w1 >> 10) & 15u;
            const uint32_t n_words = w1 & 0x3FFu;
            const uint32_t c_off = data_words_off + n_words * 4u;
            if (recv_mode >= 2u && c_off + 8u <= 0x100u) {
                const uint32_t c_lo = rd32(m + c_off);
                const uint32_t c_hi = rd32(m + c_off + 4u);
                const uint64_t c_addr = (uint64_t)c_lo | ((uint64_t)(c_hi & 0xFFFFu) << 32);
                const uint64_t c_size = (uint64_t)(c_hi >> 16);
                if (c_addr || c_size) {
                    *out_buf = c_addr;
                    *out_size = c_size;
                }
            }
        }
    }
    return m + aligned;
}

// Write a CMIF domain answer into the 0x100-byte IPC buffer (matches rom.c answer()).
static void ipc_answer(uint8_t *m, uint32_t result, const uint8_t *raw, uint32_t raw_size, uint32_t object) {
    for (uint32_t k = 0; k < 0x100u; k++) m[k] = 0;
    uint8_t *d = m + 0x10;
    wr32(d, object ? 1u : 0u);
    d += 16;
    wr32(d, 0x4F434653u);  // "SFCO"
    wr32(d + 8, result);
    d += 16;
    for (uint32_t k = 0; k < raw_size; k++) d[k] = raw[k];
    d += raw_size;
    if (object) {
        wr32(d, object);
        d += 4;
    }
    wr32(m + 4, (uint32_t)(d - m - 8 + 8 + 3) / 4u);
}

static uint32_t slot_alloc(uint32_t *mask, uint32_t base) {
    for (;;) {
        uint32_t cur = __atomic_load_n(mask, __ATOMIC_ACQUIRE);
        uint32_t slot = PRESEL_SLOTS;
        for (uint32_t k = 0; k < PRESEL_SLOTS; k++) {
            if (!(cur & (1u << k))) {
                slot = k;
                break;
            }
        }
        if (slot == PRESEL_SLOTS) return 0;
        if (__atomic_compare_exchange_n(mask, &cur, cur | (1u << slot), 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return base + slot;
    }
}

static void slot_free(uint32_t *mask, uint32_t slot) {
    __atomic_fetch_and(mask, ~(1u << slot), __ATOMIC_RELEASE);
}

static int slot_active(uint32_t *mask, uint32_t slot) {
    return (__atomic_load_n(mask, __ATOMIC_ACQUIRE) & (1u << slot)) != 0;
}

static void build_payload(uint8_t out[NX2_PRESEL_SIZE]) {
    for (uint32_t k = 0; k < NX2_PRESEL_SIZE; k++) out[k] = 0;
    wr32(out + 0x00, NX2_PRESEL_MAGIC);
    out[0x04] = 1u;
    wr64(out + 0x08, __atomic_load_n(&g_uid_lo, __ATOMIC_RELAXED));
    wr64(out + 0x10, __atomic_load_n(&g_uid_hi, __ATOMIC_RELAXED));
}

void nx2_preselected_reset(int clear_uid) {
    __atomic_store_n(&g_tracked_proxy, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_app_funcs, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_storage_mask, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_accessor_mask, 0u, __ATOMIC_RELEASE);
    if (clear_uid) {
        __atomic_store_n(&g_uid_valid, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_uid_lo, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&g_uid_hi, 0u, __ATOMIC_RELAXED);
    }
}

void nx2_preselected_set(uint64_t uid_lo, uint64_t uid_hi) {
    if (!uid_lo && !uid_hi) return;
    __atomic_store_n(&g_uid_lo, uid_lo, __ATOMIC_RELAXED);
    __atomic_store_n(&g_uid_hi, uid_hi, __ATOMIC_RELAXED);
    __atomic_store_n(&g_uid_valid, 1, __ATOMIC_RELEASE);
}

int nx2_preselected_has(void) {
    return __atomic_load_n(&g_uid_valid, __ATOMIC_ACQUIRE);
}

uint32_t nx2_preselected_app_funcs(void) {
    return __atomic_load_n(&g_app_funcs, __ATOMIC_ACQUIRE);
}

int nx2_preselected_before(uint8_t *m, uint32_t proxy_obj, int *inflight) {
    if (inflight) *inflight = NX2_PRESEL_INFLIGHT_NONE;
    const uint32_t type = rd32(m) & 0xFFFFu;
    if (type != 4u && type != 6u) return 0;

    uint64_t buf = 0, buf_size = 0;
    const uint8_t *d = ipc_parse(m, &buf, &buf_size);
    if (!d || d + 8u > m + 0x100u) return 0;

    const uint32_t kind = d[0];
    const uint32_t object = rd32(d + 4);

    // Virtual IStorage object (Open -> virtual IStorageAccessor; Close)
    if (object >= PRESEL_STORAGE_BASE && object < PRESEL_STORAGE_BASE + PRESEL_SLOTS) {
        const uint32_t slot = object - PRESEL_STORAGE_BASE;
        if (kind == 2u) {
            slot_free(&g_storage_mask, slot);
            ipc_answer(m, 0u, NULL, 0u, 0u);
            return 1;
        }
        if (kind == 1u && slot_active(&g_storage_mask, slot) && d + 32u <= m + 0x100u && rd32(d + 16) == 0x49434653u) {
            const uint32_t cmd = rd32(d + 24);
            if (cmd == 0u) {  // IStorage::Open
                const uint32_t acc = slot_alloc(&g_accessor_mask, PRESEL_ACCESSOR_BASE);
                ipc_answer(m, acc ? 0u : NX2_PRESEL_RESULT_UNAVAIL, NULL, 0u, acc);
                return 1;
            }
        }
        ipc_answer(m, NX2_PRESEL_RESULT_INVALID, NULL, 0u, 0u);
        return 1;
    }

    // Virtual IStorageAccessor object (GetSize -> 0x88; Read -> PreselectedUser slice; Close)
    if (object >= PRESEL_ACCESSOR_BASE && object < PRESEL_ACCESSOR_BASE + PRESEL_SLOTS) {
        const uint32_t slot = object - PRESEL_ACCESSOR_BASE;
        if (kind == 2u) {
            slot_free(&g_accessor_mask, slot);
            ipc_answer(m, 0u, NULL, 0u, 0u);
            return 1;
        }
        if (kind == 1u && slot_active(&g_accessor_mask, slot) && d + 32u <= m + 0x100u && rd32(d + 16) == 0x49434653u) {
            const uint32_t cmd = rd32(d + 24);
            if (cmd == 0u) {  // IStorageAccessor::GetSize
                uint8_t out[8];
                wr64(out, (uint64_t)NX2_PRESEL_SIZE);
                ipc_answer(m, 0u, out, sizeof out, 0u);
                return 1;
            }
            if (cmd == 11u) {  // IStorageAccessor::Read(s64 offset)
                if (d + 40u > m + 0x100u) {
                    ipc_answer(m, NX2_PRESEL_RESULT_INVALID, NULL, 0u, 0u);
                    return 1;
                }
                const int64_t offset = (int64_t)rd64(d + 32);
                if (offset < 0 || (uint64_t)offset > NX2_PRESEL_SIZE ||
                    (buf_size > 0 && (uint64_t)offset >= NX2_PRESEL_SIZE) ||
                    (buf_size > 0 && buf == 0)) {
                    ipc_answer(m, NX2_PRESEL_RESULT_OOB, NULL, 0u, 0u);
                    return 1;
                }
                uint8_t payload[NX2_PRESEL_SIZE];
                build_payload(payload);
                const uint64_t avail = NX2_PRESEL_SIZE - (uint64_t)offset;
                const uint64_t copy = buf_size < avail ? buf_size : avail;
                uint8_t *dst = (uint8_t *)(uintptr_t)buf;
                for (uint64_t k = 0; k < copy; k++) dst[k] = payload[(uint64_t)offset + k];
                ipc_answer(m, 0u, NULL, 0u, 0u);
                return 1;
            }
        }
        ipc_answer(m, NX2_PRESEL_RESULT_INVALID, NULL, 0u, 0u);
        return 1;
    }

    // Track real appletOE domain object lifecycle (always pass-through)
    if (kind == 2u && object) {
        uint32_t expected = object;
        __atomic_compare_exchange_n(&g_app_funcs, &expected, 0u, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
        expected = object;
        __atomic_compare_exchange_n(&g_tracked_proxy, &expected, 0u, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
        return 0;
    }

    if (kind == 1u && d + 32u <= m + 0x100u && rd32(d + 16) == 0x49434653u && inflight) {
        const uint32_t cmd = rd32(d + 24);
        const uint32_t eff_proxy = proxy_obj ? proxy_obj : __atomic_load_n(&g_tracked_proxy, __ATOMIC_ACQUIRE);
        if (!eff_proxy && (cmd == 0u || cmd == 10u)) {
            *inflight = NX2_PRESEL_INFLIGHT_OPEN_PROXY;
        } else if (eff_proxy && object == eff_proxy && cmd == 20u) {
            *inflight = NX2_PRESEL_INFLIGHT_GET_APP_FUNCS;
        } else if (cmd == 1u && d + 36u <= m + 0x100u) {
            const uint32_t cur_af = __atomic_load_n(&g_app_funcs, __ATOMIC_ACQUIRE);
            if (cur_af && object == cur_af && rd32(d + 32) == 2u)
                *inflight = NX2_PRESEL_INFLIGHT_POP_USER;
        }
    }
    return 0;
}

uint32_t nx2_preselected_after(uint8_t *m, int inflight) {
    if (inflight == NX2_PRESEL_INFLIGHT_NONE) return 0;

    const uint8_t *d = ipc_parse(m, NULL, NULL);
    if (!d || d + 32u > m + 0x100u) return 0;
    if (rd32(d + 16) != 0x4F434653u) return 0;  // "SFCO"

    const uint32_t result = rd32(d + 24);
    if (inflight == NX2_PRESEL_INFLIGHT_OPEN_PROXY) {
        if (!result && d + 36u <= m + 0x100u && rd32(d) >= 1u) {
            const uint32_t obj = rd32(d + 32);
            if (obj) __atomic_store_n(&g_tracked_proxy, obj, __ATOMIC_RELEASE);
        }
        return 0;
    }
    if (inflight == NX2_PRESEL_INFLIGHT_GET_APP_FUNCS) {
        if (!result && d + 36u <= m + 0x100u && rd32(d) >= 1u) {
            const uint32_t obj = rd32(d + 32);
            if (obj) __atomic_store_n(&g_app_funcs, obj, __ATOMIC_RELEASE);
        }
        return 0;
    }
    if (inflight == NX2_PRESEL_INFLIGHT_POP_USER) {
        if (result == NX2_PRESEL_RESULT_UNAVAIL && nx2_preselected_has()) {
            const uint32_t id = slot_alloc(&g_storage_mask, PRESEL_STORAGE_BASE);
            if (id) {
                ipc_answer(m, 0u, NULL, 0u, id);
                return id;
            }
        }
    }
    return 0;
}
