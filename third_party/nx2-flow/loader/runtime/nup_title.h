// Canonical Title ID resolution shared between the game loader (loader/runtime/game.c) and the menu (menu/source/nup.cpp).
// Both sides must derive the same /nx2-flow/<title id>/ folder for every NUP container:
//   1. A non-zero base package (kind 0) title ID is used as-is (preserving Switch 2 Edition program1 IDs such as 0400...001).
//   2. Otherwise, a non-zero update package (kind 1) title ID has its Horizon patch bits masked (& ~0xFFF: ...800 -> ...000).
//   3. If all package TOC title IDs are zero, fall back to main.npdm's ACI0 ProgramId (META +0x70 -> ACI0 +0x10).
#ifndef NX2_NUP_TITLE_H
#define NX2_NUP_TITLE_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t nx2__title_r32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint64_t nx2__title_r64(const uint8_t *p) {
    return (uint64_t)nx2__title_r32(p) | (uint64_t)nx2__title_r32(p + 4) << 32;
}

static inline uint64_t nx2_npdm_program_id(const uint8_t *npdm, size_t size) {
    if (!npdm || size < 0x80) return 0;
    if (npdm[0] != 'M' || npdm[1] != 'E' || npdm[2] != 'T' || npdm[3] != 'A') return 0;
    const uint64_t aci_off = nx2__title_r32(npdm + 0x70);
    const uint64_t aci_size = nx2__title_r32(npdm + 0x74);
    if (aci_size < 0x18 || aci_off > (uint64_t)size || aci_size > (uint64_t)size - aci_off) return 0;
    const uint8_t *aci = npdm + aci_off;
    if (aci[0] != 'A' || aci[1] != 'C' || aci[2] != 'I' || aci[3] != '0') return 0;
    return nx2__title_r64(aci + 0x10);
}

static inline uint64_t nx2_resolve_title_id(uint64_t base_title, uint64_t update_title, uint64_t npdm_program_id) {
    if (base_title) return base_title;
    if (update_title) return update_title & ~0xFFFull;
    return npdm_program_id;
}

#endif
