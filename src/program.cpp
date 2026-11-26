#include "nx2link/program.hpp"
#include "nx2link/crypto.hpp"
#include "zstd.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nx2link {
namespace {
constexpr uint64_t Limit=256ull*1024*1024;
[[noreturn]] void bad(const std::string& why){throw std::runtime_error(why);}
void bounds(uint64_t at,uint64_t size,uint64_t limit){if(at>limit||size>limit-at)bad("Invalid range/truncated program data");}
uint64_t le(const uint8_t* b,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[i])<<(8*i);return v;}
std::string hex(const uint8_t* data,size_t n){std::ostringstream s;s<<std::hex<<std::setfill('0');for(size_t i=0;i<n;++i)s<<std::setw(2)<<unsigned(data[i]);return s.str();}
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
        bool base=false;uint64_t title=0;
        for(auto p:inv_.packages)if(p.kind==0){if(base&&title!=p.title)bad("Multiple base titles not supported");base=true;title=p.title;}
        if(!base)bad("A base package is required in this version");
        Package selected{};std::map<std::string,uint64_t> chosen;bool found=false;
        for(size_t i=0;i<inv_.packages.size();++i) {
            auto p=inv_.packages[i];if(p.kind>1||p.title!=title)continue;
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
};
void lz4(const uint8_t* src,size_t count,uint8_t* out,size_t size) {
    size_t ip=0,op=0;
    auto length=[&](size_t n){if(n==15){unsigned b;do{if(ip>=count)bad("Truncated LZ4 length");b=src[ip++];if(n>size||b>size-n)bad("LZ4 length exceeds segment");n+=b;}while(b==255);}return n;};
    while(ip<count) {
        unsigned token=src[ip++];size_t literal=length(token>>4);
        if(literal>count-ip||literal>size-op)bad("LZ4 literal out of range");
        std::memcpy(out+op,src+ip,literal);ip+=literal;op+=literal;
        if(ip==count)break;
        if(count-ip<2)bad("Truncated LZ4 offset");
        size_t offset=le(src+ip,2);ip+=2;
        if(!offset||offset>op)bad("Invalid LZ4 match offset");
        size_t match=length(token&15);
        if(size-op<4||match>size-op-4)bad("LZ4 match out of range");
        match+=4;
        for(size_t i=0;i<match;++i){out[op]=out[op-offset];++op;}
    }
    if(op!=size)bad("LZ4 expanded size mismatch");
}
}
PreparedModule prepare_nso(const std::vector<uint8_t>& raw) {
    if(raw.size()<0x100||std::memcmp(raw.data(),"NSO0",4))bad("Invalid NSO0 header");
    uint32_t flags=uint32_t(le(raw.data()+12,4));
    uint64_t mem[3],sizes[3],offsets[3],stored[3];
    for(unsigned i=0;i<3;++i) {
        offsets[i]=le(raw.data()+16+i*16,4);mem[i]=le(raw.data()+20+i*16,4);sizes[i]=le(raw.data()+24+i*16,4);
        stored[i]=(flags&(1u<<i))?le(raw.data()+0x60+i*4,4):sizes[i];bounds(offsets[i],stored[i],raw.size());
        if((mem[i]&0xfff)||mem[i]>Limit||sizes[i]>Limit-mem[i])bad("Invalid or too large NSO segment");
        if(i&&mem[i]<mem[i-1]+sizes[i-1])bad("Overlapping NSO memory segments");
    }
    if(mem[0]!=0||sizes[0]<8)bad("NSO text layout not supported");
    PreparedModule m{};m.image.resize(size_t(mem[2]+sizes[2]));
    m.text_size=uint32_t(sizes[0]);m.ro_offset=uint32_t(mem[1]);m.data_offset=uint32_t(mem[2]);
    for(unsigned i=0;i<3;++i) {
        const auto* src=raw.data()+offsets[i];auto* dst=m.image.data()+mem[i];
        if(flags&(1u<<i)) {
            if((flags&0x80)&&stored[i]>=4&&!std::memcmp(src,"ZBIC",4))bad("ZBIC NSO segments are not supported yet");
            lz4(src,size_t(stored[i]),dst,size_t(sizes[i]));
        } else std::memcpy(dst,src,size_t(sizes[i]));
        if(flags&(1u<<(3+i))) {
            const auto hash=digest256(dst,size_t(sizes[i]));
            if(std::memcmp(hash.data(),raw.data()+0xa0+i*32,32))bad("NSO segment SHA-256 mismatch");
        }
    }
    m.mod0=uint32_t(le(m.image.data()+4,4));bounds(m.mod0,28,m.image.size());
    if(std::memcmp(m.image.data()+m.mod0,"MOD0",4))bad("Invalid MOD0");
    auto relative=[&](unsigned offset){return int64_t(m.mod0)+int32_t(le(m.image.data()+m.mod0+offset,4));};
    auto dyn=relative(4),bss_begin=relative(8),bss_end=relative(12),object=relative(24);
    if(dyn<0||bss_begin<0||bss_end<bss_begin||object<0)bad("Invalid MOD0 relative offsets");
    bounds(uint64_t(dyn),16,m.image.size());
    m.end=std::max<uint64_t>(m.image.size()+le(raw.data()+0x3c,4),uint64_t(bss_end));
    if(m.end>1024ull*1024*1024||uint64_t(object)>=m.end)bad("NSO BSS/module object outside preparation limits");
    m.dynamic=uint32_t(dyn);m.module_object=uint32_t(object);
    for(uint32_t at=0;at+4<=m.text_size;at+=4) {
        auto word=uint32_t(le(m.image.data()+at,4));
        if((word&0xffe0001f)==0xd4000001)m.svcs.emplace_back(at,(word>>5)&0xffff);
    }
    return m;
}
unsigned prepare_program(const Input& input,const std::function<void(const std::string&)>& report) {
    Archive archive(input);auto [package,files]=archive.program();
    std::ostringstream intro;intro<<"program title="<<std::hex<<package.title<<std::dec<<" version="<<package.version<<" parts="<<input.parts().size();report(intro.str());
    std::vector<std::string> order{"main"};for(unsigned i=0;i<10;++i)order.push_back("subsdk"+std::to_string(i));order.push_back("sdk");
    for(const auto& entry:files)if(entry.first.rfind("subsdk",0)==0&&std::find(order.begin(),order.end(),entry.first)==order.end())bad("Unsupported additional ExeFS module: "+entry.first);
    unsigned count=0;
    for(const auto& name:order) {
        auto it=files.find(name);if(it==files.end())continue;
        report("Preparing "+name+" (NUP hash, decompress, NSO segment hashes)...");
        auto raw=archive.read(it->second);auto module=prepare_nso(raw);
        auto digest=digest256(module.image.data(),module.image.size());
        std::ostringstream line;line<<"module="<<name<<" image_bytes="<<module.image.size()<<" end="<<module.end<<" mod0="<<module.mod0<<" dynamic="<<module.dynamic<<" object="<<module.module_object<<" svcs="<<module.svcs.size()<<" image_sha256="<<hex(digest.data(),digest.size());report(line.str());++count;
    }
    report("PASS preparation only; no mapping, runtime hooks, game entrypoint, RomFS or save access");return count;
}
}
