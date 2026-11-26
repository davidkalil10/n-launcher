// SPDX-License-Identifier: MIT
// Setup uses the validated C++ Input. Guest-thread reads use POD + native FS only: no C++ runtime/TLS.
#include <switch.h>
#include "nx2link/input.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>
namespace {
constexpr u32 io_error=0x2ee202;
struct Piece{FsFile file;u64 start,size;};
Piece pieces[128];unsigned count;u64 length;
u32 source_lock,cache_lock;
void take(u32* lock){while(__atomic_exchange_n(lock,1,__ATOMIC_ACQUIRE))svcSleepThread(0);}
void give(u32* lock){__atomic_store_n(lock,0,__ATOMIC_RELEASE);}
void close_source(){while(count)fsFileClose(&pieces[--count].file);length=0;}
struct CacheEntry{char path[160];u64 size;u32 chunk;};
CacheEntry caches[512];u32 cache_count;
struct OpenCache{FsFile file;u32 entry,part;u64 age;bool used;};
OpenCache opened[8];u64 clock_value;
u64 le(const u8* p,unsigned n){u64 v=0;for(unsigned i=0;i<n;++i)v|=u64(p[i])<<(8*i);return v;}
void chunk_path(char* out,const char* base,u32 part){
    unsigned n=0;while(base[n]){out[n]=base[n];++n;}
    // Cache writer uses %04u; supported range is capped at 9999 during setup.
    for(unsigned div=1000;div;div/=10)out[n++]=char('0'+part/div%10);
    for(char c: {'.','b','i','n','\0'})out[n++]=c;
}
}
extern "C" int nl_input_open(const char* path){
    close_source();
    try{
        nx2link::Input input{std::filesystem::u8path(path)};nx2link::inspect(input);
        if(input.parts().size()>128)return -1;
        auto* fs=fsdevGetDeviceFileSystem("sdmc");if(!fs)return -1;
        for(const auto& p:input.parts()){
            auto text=p.path.generic_u8string();auto colon=text.find(':');auto inner=text.substr(colon==std::string::npos?0:colon+1);
            if(inner.empty()||inner[0]!='/'||inner.size()>=FS_MAX_PATH){close_source();return -1;}
            auto& piece=pieces[count];piece.start=p.start;piece.size=p.size;
            if(R_FAILED(fsFsOpenFile(fs,inner.c_str(),FsOpenMode_Read,&piece.file))){close_source();return -1;}
            ++count;
        }
        length=input.size();return 0;
    }catch(...){close_source();return -1;}
}
extern "C" u64 nl_input_size(){return length;}
extern "C" u32 nl_input_read(u64 at,void* output,u64 size){
    if(at>length||size>length-at)return io_error;
    take(&source_lock);auto* out=static_cast<u8*>(output);u32 rc=0;unsigned k=0;
    while(k<count&&at>=pieces[k].start+pieces[k].size)++k;
    while(size&&k<count){auto& p=pieces[k];u64 local=at-p.start;auto n=std::min<u64>({size,p.size-local,4u<<20});u64 got=0;
        rc=fsFileRead(&p.file,s64(local),out,n,FsReadOption_None,&got);if(rc||got!=n){if(!rc)rc=io_error;break;}at+=n;out+=n;size-=n;if(local+n==p.size)++k;
    }
    give(&source_lock);return rc?rc:size?io_error:0;
}
extern "C" int nl_cache_open(const u8* sha,u64 size){
    if(cache_count>=512)return -1;
    auto& entry=caches[cache_count];std::strcpy(entry.path,"/switch/n-launcher/content/");unsigned n=std::strlen(entry.path);
    const char* hex="0123456789abcdef";for(unsigned i=0;i<32;++i){entry.path[n++]=hex[sha[i]>>4];entry.path[n++]=hex[sha[i]&15];}entry.path[n++]='/';entry.path[n]=0;
    char path[192];std::snprintf(path,sizeof path,"%scomplete",entry.path);FsFile file{};auto* fs=fsdevGetDeviceFileSystem("sdmc");
    if(!fs||R_FAILED(fsFsOpenFile(fs,path,FsOpenMode_Read,&file)))return -1;
    u8 header[48];u64 got=0;u32 rc=fsFileRead(&file,0,header,48,FsReadOption_None,&got);fsFileClose(&file);
    if(rc||got!=48||std::memcmp(header,"NLC1",4)||le(header+8,8)!=size||std::memcmp(header+16,sha,32))return -1;
    entry.chunk=u32(le(header+4,4));entry.size=size;if(!entry.chunk||entry.chunk>(1u<<30)||size/u64(entry.chunk)>=10000)return -1;
    for(u64 off=0,part=0;off<size;off+=entry.chunk,++part){chunk_path(path,entry.path,u32(part));if(R_FAILED(fsFsOpenFile(fs,path,FsOpenMode_Read,&file)))return -1;s64 bytes=0;rc=fsFileGetSize(&file,&bytes);fsFileClose(&file);if(rc||bytes<0||u64(bytes)!=std::min<u64>(entry.chunk,size-off))return -1;}
    return int(cache_count++);
}
extern "C" u32 nl_cache_read(u32 which,u64 at,void* output,u64 size){
    if(which>=cache_count||at>caches[which].size||size>caches[which].size-at)return io_error;
    const auto& e=caches[which];take(&cache_lock);auto* out=static_cast<u8*>(output);u32 rc=0;
    while(size){u32 part=u32(at/e.chunk);u64 local=at%e.chunk;int slot=-1;
        for(int k=0;k<8;++k)if(opened[k].used&&opened[k].entry==which&&opened[k].part==part)slot=k;
        if(slot<0){for(int k=0;k<8;++k)if(!opened[k].used){slot=k;break;}if(slot<0){slot=0;for(int k=1;k<8;++k)if(opened[k].age<opened[slot].age)slot=k;}
            auto& o=opened[slot];if(o.used)fsFileClose(&o.file);o.used=false;char path[192];chunk_path(path,e.path,part);
            rc=fsFsOpenFile(fsdevGetDeviceFileSystem("sdmc"),path,FsOpenMode_Read,&o.file);if(rc)break;o.used=true;o.entry=which;o.part=part;
        }
        auto& o=opened[slot];u64 n=std::min<u64>({size,e.chunk-local,4u<<20}),got=0;rc=fsFileRead(&o.file,s64(local),out,n,FsReadOption_None,&got);if(rc||got!=n){if(!rc)rc=io_error;break;}o.age=++clock_value;at+=n;size-=n;out+=n;
    }
    give(&cache_lock);return rc;
}
