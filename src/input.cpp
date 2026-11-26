#include "nx2link/input.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace nx2link {
namespace {
[[noreturn]] void fail(const char* why) { throw std::runtime_error(why); }
bool numeric(const std::string& name) {
    return name.size() >= 2 && std::all_of(name.begin(), name.end(), [](char c){return c>='0' && c<='9';});
}
uint64_t number(const std::string& name) {
    if (name.size()>9) fail("Split index too large");
    return std::stoull(name);
}
void range(uint64_t at, uint64_t size, uint64_t limit) {
    if (at>limit || size>limit-at) fail("Truncated input or invalid offset/size");
}
uint64_t le(const uint8_t* p, unsigned n) {
    uint64_t v=0; for(unsigned i=0;i<n;++i) v|=uint64_t(p[i])<<(8*i); return v;
}
uint32_t rotate(uint32_t a,unsigned n) { return (a>>n)|(a<<(32-n)); }
std::array<uint8_t,32> sha256(const std::vector<uint8_t>& input) {
    static constexpr uint32_t k[]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint64_t blocks=(input.size()+9+63)/64;
    for(uint64_t block=0;block<blocks;++block) {
        uint32_t w[64]={};
        for(unsigned i=0;i<64;++i) {
            const uint64_t pos=block*64+i;
            uint8_t b=pos<input.size()?input[size_t(pos)]:(pos==input.size()?0x80:0);
            if(pos>=blocks*64-8) b=uint8_t((uint64_t(input.size())*8)>>(8*(blocks*64-1-pos)));
            w[i/4]|=uint32_t(b)<<(24-8*(i%4));
        }
        for(unsigned i=16;i<64;++i) {
            auto x=w[i-15],y=w[i-2];
            w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(unsigned i=0;i<64;++i) {
            uint32_t t=hh+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
            hh=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    std::array<uint8_t,32> out{};
    for(unsigned i=0;i<32;++i)out[i]=uint8_t(h[i/4]>>(24-8*(i%4)));
    return out;
}
}

Input::Input(const fs::path& source) {
    std::map<uint64_t,fs::path> ordered;
    size_t width=0;
    if(fs::is_directory(source)) {
        for(const auto& entry:fs::directory_iterator(source)) {
            auto name=entry.path().filename().u8string();
            if(!numeric(name)) continue;
            if(!entry.is_regular_file()) fail("Split part is not a regular file");
            if(width && width!=name.size()) fail("Inconsistent split numbering width");
            width=name.size();
            if(!ordered.emplace(number(name),entry.path()).second) fail("Duplicate split index");
        }
        if(ordered.empty()) fail("Split folder has no numeric parts (00, 01, ...)");
    } else if(fs::is_regular_file(source)) {
        auto suffix=source.extension().u8string();
        if(suffix.size()>1 && numeric(suffix.substr(1))) {
            width=suffix.size()-1;
            const auto prefix=source.stem().u8string()+'.';
            auto parent=source.parent_path(); if(parent.empty()) parent=".";
            for(const auto& entry:fs::directory_iterator(parent)) {
                auto name=entry.path().filename().u8string();
                if(name.rfind(prefix,0)!=0) continue;
                auto tail=name.substr(prefix.size()); if(!numeric(tail)) continue;
                if(tail.size()!=width || !entry.is_regular_file()) fail("Invalid split sibling");
                if(!ordered.emplace(number(tail),entry.path()).second) fail("Duplicate split index");
            }
        } else ordered.emplace(0,source);
    } else fail("Input does not exist or is not a file/folder");
    uint64_t expected=0;
    for(const auto& [index,path]:ordered) {
        if(index!=expected++) fail("Missing split part: numbering must start at 00 without gaps");
        auto bytes=fs::file_size(path);
        if(!bytes) fail("Empty input/split part");
        if(bytes>uint64_t(std::numeric_limits<std::streamoff>::max())-size_) fail("Input too large");
        parts_.push_back({path,size_,bytes}); size_+=bytes;
    }
}

void Input::read(uint64_t offset,void* output,size_t count) const {
    range(offset,count,size_);
    auto* out=static_cast<char*>(output);
    auto it=std::upper_bound(parts_.begin(),parts_.end(),offset,[](uint64_t at,const Part& p){return at<p.start;});
    if(it!=parts_.begin()) --it;
    while(count) {
        auto local=offset-it->start;
        size_t n=size_t(std::min<uint64_t>(count,it->size-local));
        std::ifstream file(it->path,std::ios::binary);
        if(!file) fail("Cannot open input part");
        file.seekg(static_cast<std::streamoff>(local));
        file.read(out,static_cast<std::streamsize>(n));
        if(size_t(file.gcount())!=n) fail("Short read: input changed or storage error");
        offset+=n;out+=n;count-=n;++it;
    }
}
std::vector<uint8_t> Input::read(uint64_t at,size_t bytes) const {
    std::vector<uint8_t> out(bytes); read(at,out.data(),bytes); return out;
}

Inventory inspect(const Input& in) {
    const auto h=in.read(0,0x80);
    if(std::memcmp(h.data(),"NUP\1",4)) fail("Not a supported NUP v1 container (a renamed NSP is not a NUP)");
    const auto at=le(h.data()+8,8), length=le(h.data()+16,8);
    range(at,length,in.size());
    if(at<0x80 || length<0x30 || length>64*1024*1024) fail("Invalid/unsupported TOC size (limit: 64 MiB)");
    const auto toc=in.read(at,size_t(length));
    const auto digest=sha256(toc);
    if(std::memcmp(digest.data(),h.data()+0x18,32)) fail("TOC SHA-256 mismatch");
    if(std::memcmp(toc.data(),"TOC\1",4)) fail("Invalid TOC signature");
    const auto count=le(toc.data()+4,4), dirs=le(toc.data()+8,8), files=le(toc.data()+16,8);
    if(count!=le(h.data()+0x48,4)) fail("Package count differs between header and TOC");
    uint64_t cursor=0x30;
    for(auto [n,stride]:{std::pair<uint64_t,uint64_t>{count,0x40},{dirs,0x18},{files,0x50}}) {
        if(n>(length-cursor)/stride) fail("TOC tables exceed available data");
        cursor+=n*stride;
    }
    const auto pool=le(toc.data()+24,8), pool_size=le(toc.data()+32,8);
    range(pool,pool_size,length);
    if(pool<cursor) fail("String pool overlaps TOC tables");
    const auto declared=le(h.data()+0x40,8), blob=le(h.data()+0x38,8);
    if(declared!=in.size() || blob<at+length || blob>in.size()) fail("Container size/blob offset mismatch (missing final part?)");
    Inventory result{dirs,files,{}};
    for(uint64_t i=0;i<count;++i) {
        const auto* p=toc.data()+0x30+i*0x40;
        for(unsigned off:{8u,12u,16u}) {
            auto d=le(p+off,4); if(d!=0xffffffff && d>=dirs) fail("Invalid package directory index");
        }
        range(le(p+0x20,8),le(p+0x28,4),in.size());
        result.packages.push_back({le(p,8),uint32_t(le(p+20,4)),p[24]});
    }
    return result;
}
}
