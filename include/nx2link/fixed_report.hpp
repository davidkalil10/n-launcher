// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace nx2link {
// Bounded formatter for fatal diagnostics: no allocator, stdio or locale calls.
template<std::size_t Capacity> struct FixedReport {
    char bytes[Capacity]{};std::size_t size{};
    void text(const char* s,std::size_t limit=1024){if(!s)return;while(*s&&limit--&&size<Capacity)bytes[size++]=*s++;}
    void hex(uint64_t value){text("0x");for(int shift=60;shift>=0;shift-=4)if(size<Capacity)bytes[size++]="0123456789abcdef"[(value>>shift)&15];}
    void number(uint64_t value){char temp[20];unsigned n=0;do{temp[n++]=char('0'+value%10);value/=10;}while(value);while(n&&size<Capacity)bytes[size++]=temp[--n];}
};
}
