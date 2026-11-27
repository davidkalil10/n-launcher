// Offline BCAT progress stand-in (IDeliveryCacheProgressService).
//
// When a game runs over a donor title whose NACP
// has no BCAT delivery cache storage, app-wide IBcatService::RequestSyncDeliveryCache (cmd 10100/20100) returns a
// donor-mismatch BCAT error such as 0xC47A (2122-0098, ResultInvalidOperation) and the app displays nn::err::ShowError.
// Directory-scoped 10101/20101 requests are left pass-through so games that handle synchronous 0xC47A cleanly do not
// stall waiting on a synthetic progress object.
//
// Instead of masking the error with a broken empty reply or fragile C++ symbol hooks, we let the real bcat service run
// first; only when it fails with a donor-storage/permission error (or on a stood-in bcat session) do we hand back a
// virtual IDeliveryCacheProgressService whose DeliveryCacheProgressImpl is already Done (status = 9, result = donor
// BCAT error, 0 bytes) and whose GetEvent() returns a virtual signaled readable event handle.
// Keeping RequestSyncDeliveryCache's synchronous return at 0 suppresses donor-mismatch error popups (2122-0098) while
// propagating the real BCAT error into DeliveryCacheProgressImpl::result avoids lying ResultSuccess to games that
// inspect DeliveryCacheProgress::GetResult() when MountDeliveryCacheStorage still returns 0x147A.
//
// Pure C, no libc or <switch.h> dependency: runs safely on game threads (no TLS/libc) and in standalone host unit tests.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    NX2_BCAT_SLOTS          = 16u,
    NX2_BCAT_PROG_OBJ_BASE  = 0x4E584200u, /* "NXB\0": virtual domain object IDs */
    NX2_BCAT_PROG_HND_BASE  = 0x4E584300u, /* virtual non-domain session handles (STOOD_HANDLE range) */
    NX2_BCAT_EVENT_HND_BASE = 0x4E584400u, /* virtual signaled readable event handles (STOOD_HANDLE range) */
    NX2_BCAT_STATUS_DONE    = 9u,          /* nn::bcat::DeliveryCacheProgressStatus::Done */
    NX2_BCAT_IMPL_SIZE      = 0x200u,      /* sizeof(nn::bcat::detail::DeliveryCacheProgressImpl) */
    NX2_BCAT_PTR_BUF_SIZE   = 0x400u       /* QueryPointerBufferSize (>= 0x200) */
};

typedef struct {
    uint32_t in_use;
    uint32_t is_domain;
    uint32_t event_refs;
    uint32_t event_signaled;
    uint32_t result;
} Nx2BcatSlot;

typedef struct {
    Nx2BcatSlot slots[NX2_BCAT_SLOTS];
    uint32_t next_evict;
    uint32_t lock;
} Nx2BcatState;

typedef struct {
    uint32_t type;
    int      is_domain;
    uint32_t domain_kind;  /* 1 = SendMessage, 2 = CloseVirtualHandle */
    uint32_t object_id;
    int      has_sfci;
    uint32_t cmd;
    uint32_t in_raw_off;   /* byte offset in m[] of raw input args after SFCI header (0 if none) */
    uint64_t out_buf;      /* receive buffer for GetImpl (C recv static or B recv buffer) */
    uint64_t out_buf_size;
} Nx2BcatReq;

static inline uint32_t nx2_bcat_rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void nx2_bcat_wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline void nx2_bcat_lock(Nx2BcatState *st) {
    uint32_t spins = 0;
    while (__atomic_exchange_n(&st->lock, 1u, __ATOMIC_ACQUIRE)) {
#if defined(__SWITCH__) || defined(__SWITCH2__)
        svcSleepThread(spins < 16u ? 0 : 100000);
#elif defined(__aarch64__)
        __asm__ volatile("yield");
#endif
        if (spins < 16u) spins++;
    }
}

static inline void nx2_bcat_unlock(Nx2BcatState *st) {
    __atomic_store_n(&st->lock, 0u, __ATOMIC_RELEASE);
}

static inline int nx2_bcat_is_prog_obj(uint32_t id) {
    return id >= NX2_BCAT_PROG_OBJ_BASE && id < NX2_BCAT_PROG_OBJ_BASE + NX2_BCAT_SLOTS;
}

static inline int nx2_bcat_is_prog_handle(uint32_t h) {
    return h >= NX2_BCAT_PROG_HND_BASE && h < NX2_BCAT_PROG_HND_BASE + NX2_BCAT_SLOTS;
}

static inline int nx2_bcat_is_event_handle(uint32_t h) {
    return h >= NX2_BCAT_EVENT_HND_BASE && h < NX2_BCAT_EVENT_HND_BASE + NX2_BCAT_SLOTS;
}

static inline int nx2_bcat_is_sync_cmd(uint32_t cmd) {
    /* Only intercept app-wide RequestSyncDeliveryCache (10100 / 20100), which aborts with 2122-0098
       when the donor lacks BCAT storage. Leave directory-scoped 10101 / 20101 pass-through so games
       that handle synchronous 0xC47A cleanly do not stall waiting on sync. */
    return cmd == 10100u || cmd == 20100u;
}

static inline int nx2_bcat_is_cancel_cmd(uint32_t cmd) {
    return cmd == 10200u;
}

static inline int nx2_bcat_is_offline_donor_error(uint32_t result) {
    /* Module 122 (0x7A) = BCAT donor-capability / missing-storage errors:
     *   0xC47A (2122-0098): ResultInvalidOperation (donor has no BCAT delivery cache storage)
     *   0xB47A (2122-0090): ResultNotPermitted
     *   0xA07A (2122-0080): ResultPassphraseNotSaved
     *   0x147A (2122-0010): no delivery cache storage
     */
    return result == 0xC47Au || result == 0xB47Au || result == 0xA07Au || result == 0x147Au;
}

static inline uint32_t nx2_bcat_alloc_slot(Nx2BcatState *st, int is_domain, uint32_t result) {
    nx2_bcat_lock(st);
    uint32_t chosen = NX2_BCAT_SLOTS;
    for (uint32_t k = 0; k < NX2_BCAT_SLOTS; k++) {
        if (!st->slots[k].in_use && !st->slots[k].event_refs) {
            chosen = k;
            break;
        }
    }
    if (chosen == NX2_BCAT_SLOTS) {
        for (uint32_t k = 0; k < NX2_BCAT_SLOTS; k++) {
            if (!st->slots[k].in_use) {
                chosen = k;
                break;
            }
        }
    }
    if (chosen == NX2_BCAT_SLOTS) {
        chosen = st->next_evict++ % NX2_BCAT_SLOTS;
    }
    st->slots[chosen].in_use = 1u;
    st->slots[chosen].is_domain = is_domain ? 1u : 0u;
    st->slots[chosen].event_refs = 0u;
    st->slots[chosen].event_signaled = 1u;
    st->slots[chosen].result = result;
    nx2_bcat_unlock(st);
    return chosen;
}

static inline int nx2_bcat_event_is_signaled(Nx2BcatState *st, uint32_t h) {
    if (!nx2_bcat_is_event_handle(h)) return 0;
    const uint32_t idx = h - NX2_BCAT_EVENT_HND_BASE;
    nx2_bcat_lock(st);
    const int sig = st->slots[idx].event_refs > 0u && st->slots[idx].event_signaled != 0u;
    nx2_bcat_unlock(st);
    return sig;
}

static inline void nx2_bcat_clear_event(Nx2BcatState *st, uint32_t h) {
    if (!nx2_bcat_is_event_handle(h)) return;
    const uint32_t idx = h - NX2_BCAT_EVENT_HND_BASE;
    nx2_bcat_lock(st);
    st->slots[idx].event_signaled = 0u;
    nx2_bcat_unlock(st);
}

static inline void nx2_bcat_close_event_handle(Nx2BcatState *st, uint32_t h) {
    if (!nx2_bcat_is_event_handle(h)) return;
    const uint32_t idx = h - NX2_BCAT_EVENT_HND_BASE;
    nx2_bcat_lock(st);
    if (st->slots[idx].event_refs > 0u) st->slots[idx].event_refs--;
    if (st->slots[idx].event_refs == 0u) st->slots[idx].event_signaled = 0u;
    nx2_bcat_unlock(st);
}

static inline void nx2_bcat_close_prog_handle(Nx2BcatState *st, uint32_t h) {
    if (!nx2_bcat_is_prog_handle(h)) return;
    const uint32_t idx = h - NX2_BCAT_PROG_HND_BASE;
    nx2_bcat_lock(st);
    st->slots[idx].in_use = 0u;
    nx2_bcat_unlock(st);
}

static inline int nx2_bcat_parse_req(const uint8_t *m, Nx2BcatReq *out) {
    out->type = 0u;
    out->is_domain = 0;
    out->domain_kind = 0u;
    out->object_id = 0u;
    out->has_sfci = 0;
    out->cmd = ~0u;
    out->in_raw_off = 0u;
    out->out_buf = 0u;
    out->out_buf_size = 0u;

    const uint32_t w0 = nx2_bcat_rd32(m);
    const uint32_t w1 = nx2_bcat_rd32(m + 4);
    const uint32_t type = w0 & 0xFFFFu;
    out->type = type;
    if (type == 2u) return 1; /* Close session */

    const uint32_t num_send_statics = (w0 >> 16) & 15u;
    const uint32_t num_send_buffers = (w0 >> 20) & 15u;
    const uint32_t num_recv_buffers = (w0 >> 24) & 15u;
    const uint32_t num_exch_buffers = (w0 >> 28) & 15u;
    const uint32_t num_data_words   = w1 & 0x3FFu;
    const uint32_t recv_static_mode = (w1 >> 10) & 15u;
    const uint32_t has_special      = (w1 >> 31) & 1u;

    uint32_t o = 8u;
    if (has_special) {
        if (o + 4u > 0x100u) return 0;
        const uint32_t sp = nx2_bcat_rd32(m + o);
        o += 4u;
        if (sp & 1u) o += 8u;
        o += (((sp >> 1) & 15u) + ((sp >> 5) & 15u)) * 4u;
    }
    o += num_send_statics * 8u;
    o += num_send_buffers * 12u;
    if (o > 0x100u) return 0;

    uint64_t b_addr = 0u, b_size = 0u;
    if (num_recv_buffers > 0u && o + 12u <= 0x100u) {
        const uint32_t size_lo = nx2_bcat_rd32(m + o);
        const uint32_t addr_lo = nx2_bcat_rd32(m + o + 4);
        const uint32_t hi      = nx2_bcat_rd32(m + o + 8);
        b_addr = (uint64_t)addr_lo | ((uint64_t)((hi >> 28) & 15u) << 32) | ((uint64_t)((hi >> 2) & 0x3FFFFFu) << 36);
        b_size = (uint64_t)size_lo | ((uint64_t)((hi >> 24) & 15u) << 32);
    }
    o += num_recv_buffers * 12u;
    o += num_exch_buffers * 12u;
    if (o > 0x100u) return 0;

    const uint32_t data_off = o;
    const uint32_t aligned  = (data_off + 15u) & ~15u;

    uint64_t c_addr = 0u, c_size = 0u;
    if (recv_static_mode >= 2u) {
        const uint32_t rl_off = data_off + num_data_words * 4u;
        if (rl_off + 8u <= 0x100u) {
            const uint32_t c_lo = nx2_bcat_rd32(m + rl_off);
            const uint32_t c_hi = nx2_bcat_rd32(m + rl_off + 4);
            c_addr = (uint64_t)c_lo | ((uint64_t)(c_hi & 0xFFFFu) << 32);
            c_size = (uint64_t)(c_hi >> 16);
        }
    }

    if (c_addr != 0u && c_size > 0u) {
        out->out_buf = c_addr;
        out->out_buf_size = c_size;
    } else if (b_addr != 0u && b_size > 0u) {
        out->out_buf = b_addr;
        out->out_buf_size = b_size;
    }

    if (aligned + 16u > 0x100u) return 0;

    if (type == 5u || type == 7u) {
        if (nx2_bcat_rd32(m + aligned) == 0x49434653u) {
            out->has_sfci = 1;
            out->cmd = nx2_bcat_rd32(m + aligned + 8);
            out->in_raw_off = aligned + 16u;
        }
        return 1;
    }

    if (type == 4u || type == 6u) {
        if (nx2_bcat_rd32(m + aligned) == 0x49434653u) {
            out->is_domain = 0;
            out->has_sfci = 1;
            out->cmd = nx2_bcat_rd32(m + aligned + 8);
            out->in_raw_off = aligned + 16u;
            return 1;
        }
        if (m[aligned] == 1u && aligned + 32u <= 0x100u && nx2_bcat_rd32(m + aligned + 16) == 0x49434653u) {
            out->is_domain = 1;
            out->domain_kind = 1u;
            out->object_id = nx2_bcat_rd32(m + aligned + 4);
            out->has_sfci = 1;
            out->cmd = nx2_bcat_rd32(m + aligned + 24);
            out->in_raw_off = aligned + 32u;
            return 1;
        }
        if (m[aligned] == 2u) {
            out->is_domain = 1;
            out->domain_kind = 2u;
            out->object_id = nx2_bcat_rd32(m + aligned + 4);
            return 1;
        }
    }
    return 0;
}

static inline void nx2_bcat_write_reply(uint8_t *m, int is_domain, uint32_t result, const uint8_t *raw, uint32_t raw_size,
                                        uint32_t out_object, uint32_t copy_handle, uint32_t move_handle) {
    for (uint32_t k = 0; k < 0x100u; k++) m[k] = 0u;
    uint32_t o = 8u;
    const uint32_t has_sp = (copy_handle != 0u || move_handle != 0u) ? 1u : 0u;
    if (has_sp) {
        nx2_bcat_wr32(m + 8, ((copy_handle ? 1u : 0u) << 1) | ((move_handle ? 1u : 0u) << 5));
        o = 12u;
        if (copy_handle) {
            nx2_bcat_wr32(m + o, copy_handle);
            o += 4u;
        }
        if (move_handle) {
            nx2_bcat_wr32(m + o, move_handle);
            o += 4u;
        }
    }
    const uint32_t data_off = o;
    const uint32_t aligned = (o + 15u) & ~15u;
    uint8_t *d = m + aligned;
    if (is_domain) {
        nx2_bcat_wr32(d, out_object ? 1u : 0u);
        d += 16;
    }
    nx2_bcat_wr32(d, 0x4F434653u); /* "SFCO" */
    nx2_bcat_wr32(d + 8, result);
    d += 16;
    for (uint32_t k = 0; k < raw_size; k++) d[k] = raw[k];
    d += raw_size;
    if (is_domain && out_object) {
        nx2_bcat_wr32(d, out_object);
        d += 4;
    }
    const uint32_t end_off = (uint32_t)(d - m);
    uint32_t words = ((end_off - data_off) + 3u) / 4u + 2u;
    const uint32_t max_words = (0x100u - data_off) / 4u;
    if (words > max_words) words = max_words;
    nx2_bcat_wr32(m + 4, (has_sp ? 0x80000000u : 0u) | (words & 0x3FFu));
}

static inline void nx2_bcat_reply_ok(uint8_t *m, int is_domain) {
    nx2_bcat_write_reply(m, is_domain, 0u, NULL, 0u, 0u, 0u, 0u);
}

static inline void nx2_bcat_fill_done_impl(uint8_t *dst, uint64_t size, uint32_t result) {
    const uint64_t n = size < NX2_BCAT_IMPL_SIZE ? size : NX2_BCAT_IMPL_SIZE;
    for (uint64_t k = 0; k < n; k++) dst[k] = 0u;
    if (n >= 4u) {
        /* DeliveryCacheProgressImpl: offset 0x00 = status (Done = 9), offset 0x04 = Result */
        nx2_bcat_wr32(dst, NX2_BCAT_STATUS_DONE);
    }
    if (n >= 8u) {
        nx2_bcat_wr32(dst + 4u, result);
    }
}

static inline int nx2_bcat_synthesize_sync(Nx2BcatState *st, uint8_t *m, const Nx2BcatReq *req,
                                           uint32_t progress_result, uint32_t *out_id) {
    const uint32_t idx = nx2_bcat_alloc_slot(st, req->is_domain, progress_result);
    if (req->is_domain) {
        const uint32_t obj = NX2_BCAT_PROG_OBJ_BASE + idx;
        nx2_bcat_write_reply(m, 1, 0u, NULL, 0u, obj, 0u, 0u);
        if (out_id) *out_id = obj;
    } else {
        const uint32_t hnd = NX2_BCAT_PROG_HND_BASE + idx;
        nx2_bcat_write_reply(m, 0, 0u, NULL, 0u, 0u, 0u, hnd);
        if (out_id) *out_id = hnd;
    }
    return 1;
}

static inline int nx2_bcat_try_copy_from_domain(Nx2BcatState *st, uint8_t *m, const Nx2BcatReq *req, uint32_t *out_handle) {
    if ((req->type != 5u && req->type != 7u) || !req->has_sfci || req->cmd != 1u) return 0;
    if (req->in_raw_off == 0u || req->in_raw_off + 4u > 0x100u) return 0;
    const uint32_t obj_id = nx2_bcat_rd32(m + req->in_raw_off);
    if (!nx2_bcat_is_prog_obj(obj_id)) return 0;
    const uint32_t src_idx = obj_id - NX2_BCAT_PROG_OBJ_BASE;
    nx2_bcat_lock(st);
    const uint32_t src_res = st->slots[src_idx].result;
    nx2_bcat_unlock(st);
    const uint32_t idx = nx2_bcat_alloc_slot(st, 0, src_res);
    const uint32_t hnd = NX2_BCAT_PROG_HND_BASE + idx;
    nx2_bcat_write_reply(m, 0, 0u, NULL, 0u, 0u, 0u, hnd);
    if (out_handle) *out_handle = hnd;
    return 1;
}

static inline int nx2_bcat_handle_progress(Nx2BcatState *st, uint8_t *m, const Nx2BcatReq *req, uint32_t session_handle,
                                           uint32_t *out_kernel_rc) {
    uint32_t idx = NX2_BCAT_SLOTS;
    if (req->is_domain && nx2_bcat_is_prog_obj(req->object_id)) {
        idx = req->object_id - NX2_BCAT_PROG_OBJ_BASE;
    } else if (nx2_bcat_is_prog_handle(session_handle)) {
        idx = session_handle - NX2_BCAT_PROG_HND_BASE;
    } else {
        return 0;
    }

    *out_kernel_rc = 0u;

    if (req->type == 2u) {
        nx2_bcat_lock(st);
        st->slots[idx].in_use = 0u;
        nx2_bcat_unlock(st);
        *out_kernel_rc = 0xF601u; /* ResultSessionClosed */
        return 1;
    }

    if (req->type == 5u || req->type == 7u) {
        if (req->has_sfci && req->cmd == 0u) {
            /* ConvertCurrentObjectToDomain */
            nx2_bcat_lock(st);
            st->slots[idx].is_domain = 1u;
            nx2_bcat_unlock(st);
            uint8_t raw[4];
            nx2_bcat_wr32(raw, NX2_BCAT_PROG_OBJ_BASE + idx);
            nx2_bcat_write_reply(m, 0, 0u, raw, 4u, 0u, 0u, 0u);
            return 1;
        }
        if (req->has_sfci && (req->cmd == 2u || req->cmd == 4u)) {
            /* CloneCurrentObject / CloneCurrentObjectEx */
            nx2_bcat_lock(st);
            const uint32_t src_res = st->slots[idx].result;
            nx2_bcat_unlock(st);
            const uint32_t cloned = nx2_bcat_alloc_slot(st, 0, src_res);
            nx2_bcat_write_reply(m, 0, 0u, NULL, 0u, 0u, 0u, NX2_BCAT_PROG_HND_BASE + cloned);
            return 1;
        }
        if (req->has_sfci && req->cmd == 3u) {
            /* QueryPointerBufferSize */
            uint8_t raw[2] = {(uint8_t)(NX2_BCAT_PTR_BUF_SIZE & 0xFFu), (uint8_t)(NX2_BCAT_PTR_BUF_SIZE >> 8)};
            nx2_bcat_write_reply(m, 0, 0u, raw, 2u, 0u, 0u, 0u);
            return 1;
        }
        nx2_bcat_write_reply(m, 0, 0u, NULL, 0u, 0u, 0u, 0u);
        return 1;
    }

    if (req->is_domain && req->domain_kind == 2u) {
        /* CloseVirtualHandle */
        nx2_bcat_lock(st);
        st->slots[idx].in_use = 0u;
        nx2_bcat_unlock(st);
        nx2_bcat_write_reply(m, 1, 0u, NULL, 0u, 0u, 0u, 0u);
        return 1;
    }

    if (req->has_sfci) {
        if (req->cmd == 0u) {
            /* IDeliveryCacheProgressService::GetEvent -> OutCopyHandle<KReadableEvent> */
            nx2_bcat_lock(st);
            st->slots[idx].event_refs++;
            nx2_bcat_unlock(st);
            nx2_bcat_write_reply(m, req->is_domain, 0u, NULL, 0u, 0u, NX2_BCAT_EVENT_HND_BASE + idx, 0u);
            return 1;
        }
        if (req->cmd == 1u) {
            /* IDeliveryCacheProgressService::GetImpl -> OutLargeData<DeliveryCacheProgressImpl, HipcPointer> */
            nx2_bcat_lock(st);
            const uint32_t res = st->slots[idx].result;
            nx2_bcat_unlock(st);
            if (req->out_buf != 0u && req->out_buf_size > 0u) {
                nx2_bcat_fill_done_impl((uint8_t *)(uintptr_t)req->out_buf, req->out_buf_size, res);
            }
            nx2_bcat_write_reply(m, req->is_domain, 0u, NULL, 0u, 0u, 0u, 0u);
            return 1;
        }
    }

    nx2_bcat_write_reply(m, req->is_domain, 0u, NULL, 0u, 0u, 0u, 0u);
    return 1;
}
