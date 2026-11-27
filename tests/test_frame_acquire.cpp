// SPDX-License-Identifier: MIT
#include "nx2link/frame_acquire.hpp"
#include <cassert>
#include <iostream>
struct Window{int32_t cur_slot=-1;};
struct Frame{bool has_init=true;void* buf_linear;Window* win;uint32_t num_fbs=2,stride=5120;};
int main(){
    char shadow[8];Window window;Frame fb{true,shadow,&window};uint32_t pitch=77;void* pixels=shadow;int calls=0;
    const auto timeout=[&](uint32_t slot){++calls;assert(slot==0);return 0xea01u;};
    for(int i=0;i<3;i++){assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,timeout)==0xea01u);assert(window.cur_slot==-1&&pixels==nullptr&&pitch==77);}
    assert(calls==3);
    assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,[](uint32_t slot){assert(slot==0);return 0u;})==0);
    assert(window.cur_slot==0&&pixels==shadow&&pitch==5120);
    assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,[](uint32_t slot){assert(slot==1);return 0x1234u;})==0x1234u);
    assert(window.cur_slot==0&&pixels==nullptr);
    assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,[](uint32_t slot){assert(slot==1);return 0u;})==0);
    assert(window.cur_slot==1);
    assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,[](uint32_t slot){assert(slot==0);return 0u;})==0);
    fb.num_fbs=0;calls=0;
    assert(nx2link::acquire_cpu_frame(fb,pitch,pixels,[&](uint32_t){++calls;return 0u;})==0x1759u);
    assert(calls==0&&pixels==nullptr);
    std::cout<<"PASS: timeout retains pending slot, successful retry, fatal error propagation, ring wrap and invalid framebuffer\n";
}
