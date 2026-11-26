// SPDX-License-Identifier: MIT
#include "launch_trace.hpp"
#include "fatal_trace.hpp"
#include "nx2link/version.hpp"
#include <switch.h>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
namespace nlauncher {
namespace {constexpr const char* file="sdmc:/switch/n-launcher/logs/launch.log";}
void launch_trace(const char* phase,const std::string& detail){
    diagnostic_phase(true,phase);
    diagnostic_memory();
    FILE* f=std::fopen(file,"ab");if(!f)return;
    u64 total=0,used=0;svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
    std::fprintf(f,"stage=%s total_mib=%llu used_mib=%llu %s\n",phase,static_cast<unsigned long long>(total>>20),static_cast<unsigned long long>(used>>20),detail.c_str());std::fflush(f);fsync(fileno(f));std::fclose(f);
}
bool begin_launch_trace(const std::string& path){
    mkdir("sdmc:/switch/n-launcher/logs",0777);
    const char* old="sdmc:/switch/n-launcher/logs/launch.previous.log";
    FILE* exists=std::fopen(file,"rb");if(exists){std::fclose(exists);std::remove(old);if(std::rename(file,old))return false;}
    FILE* f=std::fopen(file,"wb");if(!f)return false;
    std::fprintf(f,"N Launcher " NX2LINK_VERSION " launch time=%lld\ninput=%s\n",static_cast<long long>(std::time(nullptr)),path.c_str());std::fflush(f);fsync(fileno(f));std::fclose(f);launch_trace("launch.requested");return true;
}
}
