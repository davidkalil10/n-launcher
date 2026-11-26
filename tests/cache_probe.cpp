// SPDX-License-Identifier: MIT
#include "nx2link/cache.hpp"
#include "nx2link/stream_hash.hpp"
#include "nx2link/crypto.hpp"
#include <iostream>
#include <stdexcept>
static int run(int argc,std::filesystem::path* paths,char** args){
    try{
        if(argc<3)return 2;
        nx2link::Input input(paths[0]);unsigned count=0;bool cancel=argc>5&&std::string(args[5])=="cancel";
        auto files=nx2link::prepare_cache(input,paths[1],[&](const std::string& s,double f){++count;if(cancel)throw std::runtime_error("cancelled");std::cout<<f<<" "<<s<<"\n";},argc>3?std::stoull(args[3]):0,argc>4?std::stoull(args[4]):1024);
        std::cout<<"OK files="<<files<<" updates="<<count<<"\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    if(argc<3)return 2;
    std::filesystem::path paths[]={argv[1],argv[2]};std::vector<std::string> strings;std::vector<char*> args;
    for(int i=0;i<argc;++i)strings.push_back(std::filesystem::path(argv[i]).u8string());
    for(auto& s:strings)args.push_back(s.data());
    return run(argc,paths,args.data());
}
#else
int main(int argc,char** argv){if(argc<3)return 2;std::filesystem::path paths[]={argv[1],argv[2]};return run(argc,paths,argv);}
#endif
