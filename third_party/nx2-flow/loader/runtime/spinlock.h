// Bounded-backoff atomic spinlock safe on Horizon OS's fixed-priority scheduler.
//
// On Horizon OS, svcSleepThread(0) (YieldWithoutCoreMigration) only yields to threads of equal
// or higher priority on the same core. If a higher-priority game thread spins on svcSleepThread(0)
// waiting for a lock held by a lower-priority thread (e.g. packed_read holding asking_lock while
// sleeping for the unpacker thread), the lower-priority holder is never scheduled -> permanent
// priority-inversion livelock. Backing off to a 100 us timed sleep after 16 yields drops the
// waiter off the run queue so any lower-priority holder can run and release the lock.
#pragma once
#include <stdint.h>

#ifndef NX2_SPIN_SLEEP
#if defined(__SWITCH__) || defined(__SWITCH2__)
#include <switch.h>
#define NX2_SPIN_SLEEP(ns) svcSleepThread(ns)
#else
#define NX2_SPIN_SLEEP(ns) ((void)(ns))
#endif
#endif

static inline void nx2_spin_backoff(uint32_t *spins) {
    NX2_SPIN_SLEEP(*spins < 16u ? 0 : 100000);
    if (*spins < 16u) (*spins)++;
}

static inline void nx2_spin_lock(uint32_t *lock) {
    uint32_t spins = 0;
    while (__atomic_exchange_n(lock, 1u, __ATOMIC_ACQUIRE)) {
        nx2_spin_backoff(&spins);
    }
}

static inline void nx2_spin_unlock(uint32_t *lock) {
    __atomic_store_n(lock, 0u, __ATOMIC_RELEASE);
}
