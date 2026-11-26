// SPDX-License-Identifier: MIT
#include "fatal_trace.hpp"
#ifdef NLAUNCHER_DIAGNOSTIC
#include "nx2link/fixed_report.hpp"
#include "nx2link/version.hpp"
#include <switch.h>
extern "C" {
#include <switch2/memory/heap.h>
extern char _start[];
}
#include <atomic>
#include <exception>
#include <new>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
namespace {
FsFile report_file{};std::atomic<bool> opened{};u64 position=0;
std::atomic_flag writing=ATOMIC_FLAG_INIT;
std::atomic<const char*> worker_phase{"not started"},ui_phase{"startup"};
std::atomic<u64> heap_total{},heap_free{};
static_assert(std::atomic<const char*>::is_always_lock_free&&std::atomic<u64>::is_always_lock_free);
void write_report(const char* kind,const char* detail,ThreadExceptionDump* ctx){
    if(!opened||writing.test_and_set())return;
    nx2link::FixedReport<4096> out;
    out.text("N Launcher " NX2LINK_VERSION " diagnostic\nkind=");out.text(kind);
    out.text("\nmessage=");out.text(detail,384);
    out.text("\nimage_base=");out.hex(reinterpret_cast<uintptr_t>(_start));
    out.text("\nworker_phase=");out.text(worker_phase.load(),128);
    out.text("\nui_phase=");out.text(ui_phase.load(),128);
    out.text("\nlast_heap_total=");out.number(heap_total.load());out.text("\nlast_heap_free=");out.number(heap_free.load());
    if(ctx){
        out.text("\nexception=");out.hex(ctx->error_desc);out.text("\npc=");out.hex(ctx->pc.x);
        out.text("\nrelative_pc=");out.hex(ctx->pc.x-reinterpret_cast<uintptr_t>(_start));
        out.text("\nlr=");out.hex(ctx->lr.x);out.text("\nrelative_lr=");out.hex(ctx->lr.x-reinterpret_cast<uintptr_t>(_start));
        out.text("\nsp=");out.hex(ctx->sp.x);out.text("\nfar=");out.hex(ctx->far.x);out.text("\nesr=");out.hex(ctx->esr);
        for(unsigned i=0;i<29;++i){out.text("\nx");out.number(i);out.text("=");out.hex(ctx->cpu_gprs[i].x);}
    }
    out.text("\n---\n");
    if(R_SUCCEEDED(fsFileWrite(&report_file,s64(position),out.bytes,out.size,FsWriteOption_Flush)))position+=out.size;
    writing.clear();
}
[[noreturn]] void terminated(){
    const char* detail="no active C++ exception";
    try{auto current=std::current_exception();if(current)std::rethrow_exception(current);}
    catch(const std::bad_alloc&){detail="std::bad_alloc";write_report("terminate",detail,nullptr);std::abort();}
    catch(const std::exception& e){write_report("terminate",e.what(),nullptr);std::abort();}
    catch(...){detail="unknown C++ exception";}
    write_report("terminate",detail,nullptr);std::abort();
}
}
extern "C" {
alignas(16) u8 __nx_exception_stack[32768];
u64 __nx_exception_stack_size=sizeof(__nx_exception_stack);
void __libnx_exception_handler(ThreadExceptionDump* ctx){write_report("cpu_exception","",ctx);}
}
namespace nlauncher {
void diagnostic_phase(bool worker,const char* phase){(worker?worker_phase:ui_phase).store(phase);}
void diagnostic_memory(){heap_total.store(heapGetTotalSize());heap_free.store(heapGetFreeSize());}
bool diagnostic_start(){
    mkdir("sdmc:/switch/n-launcher/logs",0777);
    auto* sd=fsdevGetDeviceFileSystem("sdmc");if(!sd)return false;
    const char* path="/switch/n-launcher/logs/frontend-fatal.log";
    // Preserve the preceding report before opening a fresh, preallocated handle.
    FsFile previous{};
    if(R_SUCCEEDED(fsFsOpenFile(sd,path,FsOpenMode_Read,&previous))){
        fsFileClose(&previous);fsFsDeleteFile(sd,"/switch/n-launcher/logs/frontend-fatal.previous.log");
        if(R_FAILED(fsFsRenameFile(sd,path,"/switch/n-launcher/logs/frontend-fatal.previous.log")))return false;
    }
    if(R_FAILED(fsFsCreateFile(sd,path,0,0)))return false;
    if(R_FAILED(fsFsOpenFile(sd,path,FsOpenMode_Write|FsOpenMode_Append,&report_file)))return false;
    opened=true;position=0;diagnostic_memory();std::set_terminate(terminated);
    write_report("diagnostic_started","No crash recorded yet",nullptr);
    if(!position){opened=false;fsFileClose(&report_file);return false;}
    return true;
}
void diagnostic_stop(){if(opened){diagnostic_memory();write_report("diagnostic_finished","normal frontend shutdown",nullptr);opened=false;fsFileClose(&report_file);}}
}
#endif
