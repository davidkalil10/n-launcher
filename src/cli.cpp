#include "nx2link/input.hpp"
#include "nx2link/program.hpp"
#include <cstdio>
#include <exception>
#include <string>

#ifdef _WIN32
using ArgChar=wchar_t;
#define ENTRY wmain
#else
using ArgChar=char;
#define ENTRY main
#endif
int ENTRY(int argc,ArgChar** argv) {
    try {
        if(argc!=2 && argc!=3 && argc!=5) {std::fprintf(stderr,"Usage: nx2link-probe INPUT [--prepare | --read OFFSET BYTES]\n");return 2;}
        nx2link::Input input(argv[1]);
        if(argc==3 && std::filesystem::path(argv[2]).u8string()=="--prepare"){nx2link::prepare_program(input,[](const std::string& s){std::puts(s.c_str());});return 0;}
        if(argc==5 && std::filesystem::path(argv[2]).u8string()=="--read") {
            auto n=std::stoull(std::filesystem::path(argv[4]).u8string()); if(n>1024*1024) throw std::runtime_error("Probe read limit: 1 MiB");
            auto data=input.read(std::stoull(std::filesystem::path(argv[3]).u8string()),size_t(n));
            for(auto b:data) std::printf("%02x",b);
            std::puts("");return 0;
        }
        auto info=nx2link::inspect(input);
        std::printf("NUP v1 | bytes=%llu | parts=%zu | packages=%zu | dirs=%llu | files=%llu\n",
            (unsigned long long)input.size(),input.parts().size(),info.packages.size(),
            (unsigned long long)info.directories,(unsigned long long)info.files);
        for(auto p:info.packages)std::printf("%016llX version=%u kind=%u\n",(unsigned long long)p.title,p.version,p.kind);
        std::puts("TOC SHA-256 OK. Payloads not verified; game launch not implemented.");return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"ERROR: %s\n",e.what());return 1;}
}
