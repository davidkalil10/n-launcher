#include <switch.h>
#include "nx2link/loader_probe.hpp"
#include "nx2link/module_lease.hpp"
#include "nx2link/version.hpp"
#include "fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <cstdarg>

namespace nx2link {
namespace {
bool blocked=false;
void log(const char* format,...) {
    std::error_code ec;
    std::filesystem::create_directories("sdmc:/switch/nx2-link/logs",ec);
    FILE* f=std::fopen("sdmc:/switch/nx2-link/logs/loader-probe.log","ab");
    if(!f)return;
    va_list args;va_start(args,format);std::vfprintf(f,format,args);va_end(args);
    std::fputc('\n',f);std::fclose(f);
}
struct KaihenBackend final:ModuleBackend {
    Service ro{};
    uint32_t load(void* image,size_t size,void* bss,size_t bss_size,uint64_t& base)override {
        const struct{u64 pid,image,size,bss,bss_size;} in={0,(u64)image,size,(u64)bss,bss_size};
        return serviceDispatchInOut(&ro,0,in,base,.in_send_pid=true);
    }
    uint32_t unload(uint64_t base)override {
        const struct{u64 pid,address;} in={0,base};
        return serviceDispatchIn(&ro,1,in,.in_send_pid=true);
    }
    void release(void* image,void* bss)noexcept override{std::free(image);std::free(bss);}
};
bool permissions(uint64_t base) {
    for(auto [offset,expected]:{std::pair<u64,u32>{0,Perm_R},{0x1000,Perm_Rx},{0x2000,Perm_R},{0x3000,Perm_Rw},{0x4000,Perm_Rw}}) {
        MemoryInfo info{};u32 page=0;
        const auto rc=svcQueryMemory(&info,&page,base+offset);
        log("query +%llx rc=%08x perm=%x expected=%x",(unsigned long long)offset,rc,info.perm,expected);
        if(R_FAILED(rc)||info.perm!=expected)return false;
    }
    return true;
}
std::string failed(const char* phase,u32 rc) {
    char text[240];std::snprintf(text,sizeof(text),"FALHOU: %s (0x%08x).\nDetalhes: /switch/nx2-link/logs/loader-probe.log",phase,rc);
    log("FAIL %s rc=%08x",phase,rc);return text;
}
}
std::string run_loader_probe() {
    if(blocked)return "Teste bloqueado nesta execucao por falha de mapeamento.\nSaia da NRO; nao repita a chamada nesta instancia.";
    log("\nNX2 Link %s - synthetic loader proof; no game modules",NX2LINK_VERSION);
    const Handle session=smGetServiceOverride(smEncodeName("ldr:ro"));
    if(session==INVALID_HANDLE){log("FAIL: no inherited ldr:ro session");return "Servico ldr:ro nao fornecido pelo ambiente atual.\nNenhuma ativacao ou mudanca de ambiente foi tentada.";}
    KaihenBackend backend;serviceCreate(&backend.ro,session);backend.ro.own_handle=0;
    for(unsigned cycle=0;cycle<3;++cycle) {
        auto* image=static_cast<unsigned char*>(std::aligned_alloc(0x1000,sizeof(fixture_image)));
        void* bss=std::aligned_alloc(0x1000,0x1000);
        if(!image||!bss){backend.release(image,bss);return failed("alocacao",0);}
        std::memcpy(image,fixture_image,sizeof(fixture_image));std::memset(bss,0,0x1000);
        armDCacheFlush(image,sizeof(fixture_image));armDCacheFlush(bss,0x1000);
        ModuleLease lease(backend,image,bss);
        log("cycle %u: LoadNro begin",cycle+1);
        const u32 load_rc=lease.load(sizeof(fixture_image),0x1000);
        log("cycle %u: LoadNro rc=%08x base=%llx",cycle+1,load_rc,(unsigned long long)lease.base());
        if(load_rc){blocked=true;return failed("LoadNro (backing preservado)",load_rc);}
        bool ok=permissions(lease.base());
        if(ok) {
            const auto entry=reinterpret_cast<u64(*)(u64)>(lease.base()+0x1000);
            for(u32 call=0;call<2;++call) {
                const u32 seed=call?0x20:0x10;
                const u32 expected=(seed^0x1357u)+0x2468u+2*call;
                log("cycle %u: call %u begin",cycle+1,call+1);
                const u64 result=entry(seed);
                log("cycle %u: call %u result=%llx expected=%x",cycle+1,call+1,(unsigned long long)result,expected);
                if(result!=expected){ok=false;break;}
            }
            if(ok) {
                const auto data=*reinterpret_cast<volatile u32*>(lease.base()+0x3000);
                const auto zero=*reinterpret_cast<volatile u32*>(lease.base()+0x4000);
                ok=data==0x246a && zero==2;
                log("cycle %u: mapped data=%x bss=%x",cycle+1,data,zero);
            }
        }
        log("cycle %u: UnloadNro begin",cycle+1);
        const u32 unload_rc=lease.close();
        log("cycle %u: UnloadNro rc=%08x",cycle+1,unload_rc);
        if(unload_rc){blocked=true;return failed("UnloadNro (backing preservado)",unload_rc);}
        // Source heap pages may only be read/freed after successful unmapping.
        if(!ok)return failed("permissoes ou resultado do modulo",0);
        u32 data=0,zero=0;std::memcpy(&data,image+0x3000,4);std::memcpy(&zero,bss,4);
        if(data!=0x246a||zero!=2)return failed("estado devolvido apos descarregar",0);
        log("cycle %u: PASS; heap backing released after scope",cycle+1);
    }
    log("PASS 3/3 load, permissions, calls, data/BSS, unload and reload");
    return "OK: carregador 3/3 ciclos.\nCarregou, executou e descarregou o modulo de teste.\nDados e BSS conferidos. Nenhum jogo foi executado.\nLog: /switch/nx2-link/logs/loader-probe.log";
}
}
