// Fallback for appletOE IApplicationFunctions::PopLaunchParameter(PreselectedUser).
// On Switch 2 (and across chained launches in a single nx2-flow session), am's one-shot
// LaunchParameterKind_PreselectedUser (kind 2) may already be consumed or absent, causing
// PopLaunchParameter(2) to return 0x480 (am::ResultNotAvailable) and the game SDK to pass
// AccountUid {0, 0} to acc:u0::GetProfile -> 0x60a -> abort / black screen.
// Runs on game threads: no C library, no TLS, lock-free atomics only.
#pragma once
#include <stdint.h>

#define NX2_PRESEL_SIZE           0x88u
#define NX2_PRESEL_MAGIC          0xC79497CAu
#define NX2_PRESEL_RESULT_UNAVAIL 0x480u
#define NX2_PRESEL_RESULT_OOB     0x3EE80u
#define NX2_PRESEL_RESULT_INVALID 0x3E880u

enum {
    NX2_PRESEL_INFLIGHT_NONE          = 0,
    NX2_PRESEL_INFLIGHT_OPEN_PROXY    = 1,
    NX2_PRESEL_INFLIGHT_GET_APP_FUNCS = 2,
    NX2_PRESEL_INFLIGHT_POP_USER      = 3,
};

void nx2_preselected_reset(int clear_uid);
void nx2_preselected_set(uint64_t uid_lo, uint64_t uid_hi);
int nx2_preselected_has(void);
uint32_t nx2_preselected_app_funcs(void);

int nx2_preselected_before(uint8_t *m, uint32_t proxy_obj, int *inflight);
uint32_t nx2_preselected_after(uint8_t *m, int inflight);
