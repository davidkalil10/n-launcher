// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace nx2link {
// Never expose pixels or advance the ring until the display releases the image.
// In particular, a timeout must leave the same image pending on the next call.
template<class Frame, class Wait>
uint32_t acquire_cpu_frame(Frame& fb, uint32_t& stride, void*& pixels, Wait wait) {
    pixels=nullptr;
    if(!fb.has_init || !fb.buf_linear || !fb.win || !fb.num_fbs) return 0x1759u;
    const auto slot=(fb.win->cur_slot+1)%static_cast<int32_t>(fb.num_fbs);
    const uint32_t rc=wait(static_cast<uint32_t>(slot));
    if(rc) return rc;
    fb.win->cur_slot=slot;stride=fb.stride;pixels=fb.buf_linear;
    return 0;
}
}
