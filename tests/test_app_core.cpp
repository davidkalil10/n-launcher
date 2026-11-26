// SPDX-License-Identifier: MIT
#include "nx2link/update.hpp"
#include "nx2link/crypto.hpp"
#include "nx2link/http.hpp"
#include "nx2link/profile.hpp"
#include "nx2link/layout.hpp"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace nx2link;
namespace fs=std::filesystem;
static unsigned cases;
template<class F> void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}assert(threw);++cases;}
static void put(std::vector<uint8_t>& b,size_t at,uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)b.at(at+i)=uint8_t(value>>(8*i));}
static std::vector<uint8_t> fixture(const std::string& version="0.0.6"){
    std::vector<uint8_t> b(0x1000+0x38+128+0x4000);std::memcpy(b.data()+16,"NRO0",4);put(b,4,0x80,4);put(b,24,0x1000,4);put(b,0x24,0x1000,4);std::memcpy(b.data()+0x80,"MOD0",4);std::memcpy(b.data()+0x9c,"OUN0",4);put(b,0xa0,2,4);put(b,0xa4,1,4);
    size_t a=0x1000;std::memcpy(b.data()+a,"ASET",4);put(b,a+8,0x38,8);put(b,a+16,128,8);put(b,a+24,0x38+128,8);put(b,a+32,0x4000,8);b[a+0x38]=255;b[a+0x39]=216;b[a+0x3a]=255;
    auto meta=a+0x38+128;std::memcpy(b.data()+meta,"N Launcher",11);std::memcpy(b.data()+meta+0x3060,version.c_str(),version.size());return b;
}
static void write(const fs::path& p,const std::vector<uint8_t>& b){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());assert(bool(f));}
static std::vector<uint8_t> read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::string hex(const std::array<uint8_t,32>& sha){const char* h="0123456789abcdef";std::string s;for(auto b:sha){s+=h[b>>4];s+=h[b&15];}return s;}
static UpdateManifest manifest(const std::vector<uint8_t>& b){return {"0.0.6",b.size(),digest256(b.data(),b.size())};}
static std::string text(const UpdateManifest& m){return "schema=1\nversion="+m.version+"\nasset=n-launcher.nro\nsize="+std::to_string(m.size)+"\nsha256="+hex(m.sha)+"\n";}
static UpdateFetch fetch(const std::vector<uint8_t>& b){return [b](const std::string& url,const UpdateSink& sink,uint64_t limit){assert(url=="https://github.com/davidkalil10/n-launcher/releases/download/v0.0.6/n-launcher.nro");assert(limit==b.size());for(size_t at=0;at<b.size();at+=997)sink(b.data()+at,std::min<size_t>(997,b.size()-at));};}
static HttpResponse response(const std::string& wire,std::string& output,uint64_t limit=100){size_t at=0;return read_update_response([&](uint8_t* p,size_t n){auto take=std::min<size_t>({n,3,wire.size()-at});std::memcpy(p,wire.data()+at,take);at+=take;return take;},[&](const uint8_t* p,size_t n){output.append(reinterpret_cast<const char*>(p),n);},limit);}
int main(int argc,char** argv){
    assert(argc==2);auto root=fs::path(argv[1]);fs::create_directories(root);
    auto b=fixture();auto m=manifest(b);assert(parse_update_manifest(text(m)).sha==m.sha);++cases;
    assert(version_newer("0.0.10","0.0.9")&&!version_newer("0.0.6","0.0.6")&&!version_newer("0.0.5","0.0.6"));++cases;
    rejects([&]{parse_update_manifest(text(m)+"schema=1\n");});rejects([&]{parse_update_manifest("schema=1\n");});rejects([&]{version_newer("0.0.6.evil","0.0.5");});rejects([&]{version_newer("0.00.6","0.0.5");});
    rejects([&]{auto s=text(m);s.replace(s.find("n-launcher.nro"),14,"../wrong.nro");parse_update_manifest(s);});
    assert(parse_update_url(update_asset_url(m)).host=="github.com");++cases;
    for(auto url:{"http://github.com/x","https://github.com.evil/x","https://127.0.0.1/x","https://github.com:443/x","https://github.com/x\r\nInjected: y"})rejects([&]{parse_update_url(url);});
    std::string output;auto reply=response("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello",output);assert(reply.status==200&&output=="hello");++cases;
    output.clear();response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n",output);assert(output=="hello");++cases;
    rejects([&]{std::string o;response("HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nhello",o);});
    rejects([&]{std::string o;response("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello",o);});
    rejects([&]{std::string o;response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\nhello",o);});
    rejects([&]{std::string o;response("HTTP/1.1 200 OK\r\nContent-Length: 101\r\n\r\n",o,100);});
    rejects([&]{std::string o;response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n",o);});
    output.clear();reply=response("HTTP/1.1 302 Found\r\nLocation: https://release-assets.githubusercontent.com/test\r\n\r\n",output);assert(reply.status==302&&!reply.location.empty()&&output.empty());++cases;
    auto exe=root/fs::u8path("aplicativo Pokémon.nro");auto previous=exe;previous+=".previous";auto state=exe;state+=".update-state";auto part=exe;part+=".update-part";auto old=fixture("0.0.5");
    write(exe,old);write(root/"save/progress.bin",{1,2,3});write(root/"cache/0000.bin",{4,5,6});
    install_update(m,"0.0.5",exe,fetch(b),[](const std::string&,double){});assert(read(exe)==b&&read(previous)==old&&fs::exists(state));assert((read(root/"save/progress.bin")==std::vector<uint8_t>{1,2,3}));++cases;
    assert(!confirm_update(exe,"0.0.5")&&fs::exists(previous));assert(confirm_update(exe,"0.0.6")&&!fs::exists(previous)&&!fs::exists(state));++cases;
    rejects([&]{install_update(m,"0.0.6",exe,fetch(b),[](const std::string&,double){});});
    write(exe,old);auto corrupt=m;corrupt.sha[0]^=1;rejects([&]{install_update(corrupt,"0.0.5",exe,fetch(b),[](const std::string&,double){});});assert(read(exe)==old&&!fs::exists(previous)&&!fs::exists(part));
    auto wrong=b;wrong[0xa0]=1;auto bad=manifest(wrong);rejects([&]{install_update(bad,"0.0.5",exe,fetch(wrong),[](const std::string&,double){});});assert(read(exe)==old);
    rejects([&]{install_update(m,"0.0.5",exe,[](const std::string&,const UpdateSink& s,uint64_t){uint8_t p=0;s(&p,1);throw std::runtime_error("interrupted");},[](const std::string&,double){});});assert(read(exe)==old&&!fs::exists(part));
    rejects([&]{install_update(m,"0.0.5",exe,fetch(b),[](const std::string& step,double){if(step.find("Instalando")==0)throw std::runtime_error("cancelled");});});assert(read(exe)==old&&!fs::exists(state));
    write(previous,old);rejects([&]{install_update(m,"0.0.5",exe,fetch(b),[](const std::string&,double){});});assert(read(previous)==old);fs::remove(previous);
    assert(parse_memory("8192",true)==8192&&parse_memory("",false)==0&&step_memory(0,-1,true)==8992&&step_memory(8992,1,true)==0);++cases;
    rejects([&]{parse_memory("8993",true);});rejects([&]{parse_memory("64",false);});
    auto args=launch_arguments("sdmc:/switch/n-launcher/n-launcher.nro","sdmc:/Jogos com espaços/Game.nup","sdmc:/switch/n-launcher/n-launcher.nro",512);assert(args.find(" room=512")!=std::string::npos&&args.find("\"room=")==std::string::npos);++cases;
    rejects([&]{launch_arguments("x","bad\"path","y",0);});
    auto sd=root/"card";auto legacy=sd/"switch/nx2-link";write(legacy/"content/hash/0000.bin",{7,8});write(legacy/"0400000000000000/save/a",{9});write(legacy/"nx2-link-switch2.nro",old);
    {std::ofstream cfg(legacy/"menu.cfg");cfg<<"games=/switch/nx2-link/input\n";}
    auto moved=migrate_layout(sd,"sdmc:/switch/nx2-link/nx2-link-switch2.nro");assert(moved.moved&&moved.executable=="sdmc:/switch/n-launcher/nx2-link-switch2.nro");auto now=sd/"switch/n-launcher";assert((read(now/"content/hash/0000.bin")==std::vector<uint8_t>{7,8}));auto cfg=read(now/"menu.cfg");assert(std::string(cfg.begin(),cfg.end()).find("/switch/n-launcher/input")!=std::string::npos);++cases;
    assert(!migrate_layout(sd,moved.executable).moved);++cases;
    auto other=root/"both";write(other/"switch/nx2-link/menu.cfg",{1});write(other/"switch/n-launcher/menu.cfg",{2});rejects([&]{migrate_layout(other,"sdmc:/switch/n-launcher/n-launcher.nro");});assert(read(other/"switch/nx2-link/menu.cfg")[0]==1&&read(other/"switch/n-launcher/menu.cfg")[0]==2);
    auto fresh=root/"fresh";write(fresh/"switch/nx2-link/content/hash/0000.bin",{3});write(fresh/"switch/n-launcher/n-launcher.nro",b);assert(migrate_layout(fresh,"sdmc:/switch/n-launcher/n-launcher.nro").moved);assert(read(fresh/"switch/n-launcher/content/hash/0000.bin")[0]==3);++cases;
    std::cout<<"PASS "<<cases<<" app-core checks: updates, HTTPS parsing, rollback, profiles and directory migration\n";
}
