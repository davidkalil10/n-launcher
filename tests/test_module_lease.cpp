#include "nx2link/module_lease.hpp"
#include <stdexcept>
#include <cstdio>
using nx2link::ModuleLease;
void check(bool v){if(!v)throw std::runtime_error("ownership assertion failed");}
struct Fake:nx2link::ModuleBackend {
    uint32_t load_rc=0,unload_rc=0;uint64_t address=0x12340000;
    int loads=0,unloads=0,releases=0;
    uint32_t load(void*,size_t,void*,size_t,uint64_t& base)override{++loads;base=address;return load_rc;}
    uint32_t unload(uint64_t base)override{check(base==address);++unloads;return unload_rc;}
    void release(void*,void*)noexcept override{++releases;}
};
int main(){
    {Fake f;{ModuleLease m(f,nullptr,nullptr);}check(f.releases==1);}
    {Fake f;{ModuleLease m(f,nullptr,nullptr);check(!m.load(0x4000,0x1000));check(m.pinned());check(!m.close());check(!m.pinned());}check(f.releases==1&&f.loads==1&&f.unloads==1);}
    {Fake f;f.load_rc=123;{ModuleLease m(f,nullptr,nullptr);check(m.load(0x4000,0x1000)==123);}check(f.releases==0&&f.unloads==0);}
    {Fake f;f.unload_rc=456;{ModuleLease m(f,nullptr,nullptr);check(!m.load(0x4000,0x1000));check(m.close()==456);}check(f.releases==0);}
    {Fake f;f.address=0;{ModuleLease m(f,nullptr,nullptr);check(m.load(0x4000,0x1000)==ModuleLease::InvalidState);}check(f.releases==0);}
    {Fake f;{ModuleLease m(f,nullptr,nullptr);check(!m.load(0x4000,0x1000));check(m.load(0x4000,0x1000)==ModuleLease::InvalidState);check(!m.close());check(m.close()==ModuleLease::InvalidState);}check(f.loads==1&&f.unloads==1&&f.releases==1);}
    {Fake f;for(int i=0;i<3;++i){ModuleLease m(f,nullptr,nullptr);check(!m.load(0x4000,0x1000));check(!m.close());}check(f.loads==3&&f.unloads==3&&f.releases==3);}
    std::puts("PASS: lease ownership, mapping/unmapping failures, zero base, duplicate calls and reload cycles");
}
