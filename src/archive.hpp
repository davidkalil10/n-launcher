// SPDX-License-Identifier: MIT
// Shared checked TOC traversal for program preparation and streaming RomFS cache.
#pragma once
#include "nx2link/input.hpp"
#include "nx2link/crypto.hpp"
#include "zstd.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
namespace nx2link::archive_detail {
[[noreturn]] inline void bad(const std::string& why){throw std::runtime_error(why);}
inline void bounds(uint64_t at,uint64_t size,uint64_t limit){if(at>limit||size>limit-at)bad("Invalid range/truncated archive data");}
inline uint64_t le(const uint8_t* b,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[i])<<(8*i);return v;}
constexpr uint64_t Limit=256ull*1024*1024;
struct File{uint64_t size,at,stored;std::array<uint8_t,32> hash;uint8_t codec;};
class Archive {
    const Input& input_;
    Inventory inv_;
    std::vector<uint8_t> toc_;
    uint64_t dirs_,files_,children_,pool_,pool_size_,blob_;
    std::string name(uint64_t at,uint64_t len)const {
        bounds(at,len,pool_size_);if(!len||len>1024)bad("Invalid program filename");
        std::string s(reinterpret_cast<const char*>(toc_.data()+pool_+at),size_t(len));
        if(s=="."||s==".."||s.find_first_of("/\\:\0",0,4)!=std::string::npos)bad("Unsafe program filename");
        return s;
    }
    void walk(uint32_t dir,const std::string& path,std::set<uint32_t>& active,std::map<std::string,uint64_t>& out)const {
        if(dir>=inv_.directories||active.size()>=32||!active.insert(dir).second)bad("Invalid/cyclic directory graph");
        const auto* d=toc_.data()+dirs_+uint64_t(dir)*0x18;
        uint64_t first=le(d+8,4),count=le(d+12,4);
        if(count>100000||first>(pool_-children_)/4||count>(pool_-children_)/4-first)bad("Directory children outside TOC");
        for(uint64_t i=0;i<count;++i) {
            uint64_t child=le(toc_.data()+children_+(first+i)*4,4);
            if(child<inv_.directories) {
                auto* next=toc_.data()+dirs_+child*0x18;
                walk(uint32_t(child),path+name(le(next,4),le(next+4,4))+"/",active,out);
            } else {
                child-=inv_.directories;if(child>=inv_.files)bad("Invalid program file index");
                auto* f=toc_.data()+files_+child*0x50;
                if(!out.emplace(path+name(le(f,4),le(f+4,4)),child).second)bad("Duplicate program path");
            }
        }
        active.erase(dir);
    }
    uint64_t application_title()const {
        bool base=false;uint64_t title=0;
        for(auto p:inv_.packages)if(p.kind==0){if(base&&title!=p.title)bad("Multiple base titles not supported");base=true;title=p.title;}
        if(base)return title;
        // Match the generic runtime: a self-contained kind=1 package can be opened
        // without a kind=0 label. This does not supply files absent from the NUP.
        bool update=false;
        for(auto p:inv_.packages)if(p.kind==1){
            if(update&&(title&~0xfffull)!=(p.title&~0xfffull))bad("Update packages belong to different titles");
            if(!update)title=p.title;
            update=true;
        }
        if(!update)bad("No application or update package in this NUP");
        return title;
    }
public:
    explicit Archive(const Input& input):input_(input),inv_(inspect(input)) {
        const auto h=input.read(0,0x80);
        const auto toc_at=le(h.data()+8,8),toc_size=le(h.data()+16,8);
        if(toc_size<0x30||toc_size>64*1024*1024)bad("TOC changed or exceeds preparation limit");
        bounds(toc_at,toc_size,input.size());
        toc_=input.read(toc_at,size_t(toc_size));
        auto digest=digest256(toc_.data(),toc_.size());
        if(std::memcmp(digest.data(),h.data()+0x18,32))bad("TOC changed after inspection");
        if(le(toc_.data()+4,4)!=inv_.packages.size()||le(toc_.data()+8,8)!=inv_.directories||le(toc_.data()+16,8)!=inv_.files)bad("TOC counts changed during inspection");
        dirs_=0x30+inv_.packages.size()*0x40;files_=dirs_+inv_.directories*0x18;
        children_=files_+inv_.files*0x50;pool_=le(toc_.data()+24,8);pool_size_=le(toc_.data()+32,8);blob_=le(h.data()+0x38,8);
        bounds(pool_,pool_size_,toc_.size());if(pool_<children_)bad("TOC string pool overlaps tables");
    }
    std::pair<Package,std::map<std::string,uint64_t>> program()const {
        const uint64_t title=application_title();
        Package selected{};std::map<std::string,uint64_t> chosen;bool found=false;
        for(size_t i=0;i<inv_.packages.size();++i) {
            auto p=inv_.packages[i];if(p.kind>1||(p.title&~0xfffull)!=(title&~0xfffull))continue;
            auto dir=le(toc_.data()+0x30+i*0x40+12,4);if(dir==0xffffffff)continue;
            std::map<std::string,uint64_t> entries;std::set<uint32_t> active;walk(uint32_t(dir),"",active,entries);
            if(!entries.count("main"))continue;
            if(!found||p.version>selected.version||(p.version==selected.version&&p.kind>=selected.kind)){selected=p;chosen=std::move(entries);found=true;}
        }
        if(!found)bad("No main module in matching program packages");
        return {selected,std::move(chosen)};
    }
    std::vector<uint8_t> read(uint64_t index)const {
        if(index>=inv_.files)bad("File index outside TOC");
        const auto* f=toc_.data()+files_+index*0x50;
        File file{le(f+8,8),le(f+16,8),le(f+24,8),{},f[0x40]};std::memcpy(file.hash.data(),f+32,32);
        if(file.size>Limit||file.stored>Limit)bad("Program file exceeds 256 MiB preparation limit");
        bounds(file.at,file.stored,input_.size());if(file.at<blob_)bad("Program payload overlaps metadata");
        std::vector<uint8_t> bytes(size_t(std::max<uint64_t>(1,file.size)));
        if(file.codec==0) {
            if(file.stored!=file.size)bad("Uncompressed file size mismatch");
            input_.read(file.at,bytes.data(),size_t(file.size));
        } else if(file.codec==1) {
            std::unique_ptr<ZSTD_DCtx,decltype(&ZSTD_freeDCtx)> ctx(ZSTD_createDCtx(),ZSTD_freeDCtx);
            if(!ctx)bad("Cannot allocate Zstd decoder");
            if(ZSTD_isError(ZSTD_DCtx_setParameter(ctx.get(),ZSTD_d_windowLogMax,26)))bad("Cannot set Zstd window limit");
            std::vector<uint8_t> chunk(256*1024);uint64_t at=0,produced=0;size_t rc=1;
            while(at<file.stored) {
                size_t count=size_t(std::min<uint64_t>(chunk.size(),file.stored-at));input_.read(file.at+at,chunk.data(),count);at+=count;
                ZSTD_inBuffer in{chunk.data(),count,0};
                while(in.pos<in.size) {
                    ZSTD_outBuffer out{bytes.data()+produced,size_t(file.size-produced),0};auto before=in.pos;
                    rc=ZSTD_decompressStream(ctx.get(),&out,&in);
                    if(ZSTD_isError(rc))bad(std::string("Zstd: ")+ZSTD_getErrorName(rc));
                    produced+=out.pos;
                    if(!rc&&(at!=file.stored||in.pos!=in.size))bad("Trailing data after Zstd frame");
                    if(!out.pos&&before==in.pos)bad("Zstd output size mismatch");
                }
            }
            if(rc||produced!=file.size)bad("Truncated Zstd frame or incorrect expanded size");
        } else bad("Unsupported NUP file codec");
        bytes.resize(size_t(file.size));auto hash=digest256(bytes.data(),bytes.size());
        if(hash!=file.hash)bad("Program file SHA-256 mismatch");
        return bytes;
    }
    File file(uint64_t index) const {
        if(index>=inv_.files)bad("File index outside TOC");
        const auto* f=toc_.data()+files_+index*0x50;
        File result{le(f+8,8),le(f+16,8),le(f+24,8),{},f[0x40]};
        std::memcpy(result.hash.data(),f+32,32);
        bounds(result.at,result.stored,input_.size());
        if(result.at<blob_ || result.codec>1)bad("Invalid payload offset/codec");
        return result;
    }
    std::map<std::string,uint64_t> romfs() const {
        std::vector<size_t> layers;const uint64_t title=application_title();
        for(size_t i=0;i<inv_.packages.size();++i){auto p=inv_.packages[i];if(p.kind<=1 && (p.title&~0xfffull)==(title&~0xfffull))layers.push_back(i);}
        if(layers.size()>8)bad("More than eight base/update layers are not supported by the current runtime");
        std::stable_sort(layers.begin(),layers.end(),[&](size_t a,size_t b){auto x=inv_.packages[a],y=inv_.packages[b];return x.kind!=y.kind?x.kind<y.kind:x.version<y.version;});
        std::map<std::string,uint64_t> result;
        for(auto i:layers){auto dir=le(toc_.data()+0x30+i*0x40+16,4);if(dir==0xffffffff)continue;
            std::map<std::string,uint64_t> entries;std::set<uint32_t> active;walk(uint32_t(dir),"",active,entries);
            for(auto& item:entries)result[item.first]=item.second;
        }
        return result;
    }
};
}
