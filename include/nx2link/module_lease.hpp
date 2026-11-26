#pragma once
#include <cstddef>
#include <cstdint>

namespace nx2link {
struct ModuleBackend {
    virtual ~ModuleBackend() = default;
    virtual uint32_t load(void* image,size_t size,void* bss,size_t bss_size,uint64_t& base)=0;
    virtual uint32_t unload(uint64_t base)=0;
    virtual void release(void* image,void* bss) noexcept=0;
};
// Mapping errors can leave a partial mapping. Never release its heap backing.
class ModuleLease {
public:
    ModuleLease(ModuleBackend& backend,void* image,void* bss):backend_(backend),image_(image),bss_(bss){}
    ModuleLease(const ModuleLease&)=delete;
    ModuleLease& operator=(const ModuleLease&)=delete;
    ~ModuleLease(){if(!pinned_)backend_.release(image_,bss_);}
    uint32_t load(size_t size,size_t bss_size) {
        if(attempted_)return InvalidState;
        attempted_=true;pinned_=true;
        uint32_t rc=backend_.load(image_,size,bss_,bss_size,base_);
        loaded_=rc==0 && base_!=0;
        return rc?rc:(loaded_?0:InvalidState);
    }
    uint32_t close() {
        if(!loaded_)return InvalidState;
        uint32_t rc=backend_.unload(base_);
        if(!rc){pinned_=false;loaded_=false;base_=0;}
        return rc;
    }
    uint64_t base()const{return base_;}
    bool pinned()const{return pinned_;}
    static constexpr uint32_t InvalidState=0xffffffffu;
private:
    ModuleBackend& backend_;
    void *image_,*bss_;
    uint64_t base_=0;
    bool attempted_=false,loaded_=false,pinned_=false;
};
}
