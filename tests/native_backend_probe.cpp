// SPDX-License-Identifier: MIT
#include <switch.h>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
static std::filesystem::path card;static std::string drive;
FsFileSystem* fsdevGetDeviceFileSystem(const char*){static FsFileSystem fs;return &fs;}
u32 fsFsOpenFile(FsFileSystem*,const char* name,int,FsFile* f){
    std::string path=name;
    auto real=path.find("/switch/n-launcher/content/")==0?card/std::filesystem::u8path(path.substr(1)):std::filesystem::u8path(drive+path);
#ifdef _WIN32
    f->file=_wfopen(real.c_str(),L"rb");
#else
    f->file=std::fopen(real.c_str(),"rb");
#endif
    return f->file?0:1;
}
u32 fsFileRead(FsFile* f,s64 at,void* out,u64 n,int,u64* got){
#ifdef _WIN32
    if(_fseeki64(f->file,at,SEEK_SET))return 1;
#else
    if(fseeko(f->file,at,SEEK_SET))return 1;
#endif
    *got=std::fread(out,1,size_t(n),f->file);return 0;
}
u32 fsFileGetSize(FsFile* f,s64* size){
#ifdef _WIN32
    _fseeki64(f->file,0,SEEK_END);*size=_ftelli64(f->file);
#else
    fseeko(f->file,0,SEEK_END);*size=ftello(f->file);
#endif
    return *size<0?1:0;
}
void fsFileClose(FsFile* f){std::fclose(f->file);f->file=nullptr;}
extern "C" int nl_input_open(const char*);
extern "C" u32 nl_input_read(u64,void*,u64);
extern "C" int nl_cache_open(const u8*,u64);
extern "C" u32 nl_cache_read(u32,u64,void*,u64);
int main(int argc,char** argv){
    if(argc<5)return 2;
    std::string mode=argv[1];u32 rc=1;std::vector<u8> bytes;
    if(mode=="input"){
        drive=std::filesystem::u8path(argv[2]).root_name().string();if(nl_input_open(argv[2]))return 1;
        auto at=std::stoull(argv[3]),n=std::stoull(argv[4]);if(n>1000000)return 2;bytes.resize(size_t(n));rc=nl_input_read(at,bytes.data(),n);
    }else if(mode=="cache"&&argc==7){
        card=std::filesystem::u8path(argv[2]);std::string hex=argv[3];if(hex.size()!=64)return 2;u8 sha[32];for(unsigned i=0;i<32;++i)sha[i]=u8(std::stoul(hex.substr(i*2,2),nullptr,16));
        int slot=nl_cache_open(sha,std::stoull(argv[4]));if(slot<0)return 1;auto at=std::stoull(argv[5]),n=std::stoull(argv[6]);if(n>1000000)return 2;bytes.resize(size_t(n));rc=nl_cache_read(u32(slot),at,bytes.data(),n);
    }
    if(rc)return 1;
    const char* hex="0123456789abcdef";for(auto b:bytes)std::cout<<hex[b>>4]<<hex[b&15];return 0;
}
