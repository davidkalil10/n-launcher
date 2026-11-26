// SPDX-License-Identifier: MIT
#include "nx2link/cache.hpp"
#include "nx2link/stream_hash.hpp"
#include "archive.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#ifdef __SWITCH__
#include <switch.h>
#endif
namespace nx2link {
namespace {
using archive_detail::File;using archive_detail::bad;
std::string hex(const std::array<uint8_t,32>& bytes){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:bytes)s<<std::setw(2)<<unsigned(b);return s.str();}
std::string part_name(uint64_t i){char name[32];std::snprintf(name,sizeof name,"%04llu.bin",static_cast<unsigned long long>(i));return name;}
std::array<uint8_t,48> marker(const File& f,uint64_t chunk){
    std::array<uint8_t,48> out{};std::memcpy(out.data(),"NLC1",4);
    for(unsigned i=0;i<4;++i)out[4+i]=uint8_t(chunk>>(8*i));
    for(unsigned i=0;i<8;++i)out[8+i]=uint8_t(f.size>>(8*i));
    std::copy(f.hash.begin(),f.hash.end(),out.begin()+16);return out;
}
bool ready(const std::filesystem::path& dir,const File& f,uint64_t chunk){
    std::ifstream in(dir/"complete",std::ios::binary);std::array<uint8_t,48> bytes{};in.read(reinterpret_cast<char*>(bytes.data()),48);
    if(in.gcount()!=48||bytes!=marker(f,chunk))return false;
    for(uint64_t off=0,i=0;off<f.size;off+=chunk,++i){std::error_code ec;auto size=std::filesystem::file_size(dir/part_name(i),ec);if(ec||size!=std::min(chunk,f.size-off))return false;}
    return true;
}
void unpack(const Input& input,const File& f,const std::filesystem::path& dir,uint64_t chunk,const std::function<void(uint64_t)>& report){
    std::filesystem::create_directories(dir);std::filesystem::remove(dir/"complete");
    std::unique_ptr<ZSTD_DCtx,decltype(&ZSTD_freeDCtx)> z(ZSTD_createDCtx(),ZSTD_freeDCtx);if(!z)bad("Sem memória para descompressão");
    if(ZSTD_isError(ZSTD_DCtx_setParameter(z.get(),ZSTD_d_windowLogMax,26)))bad("Falha ao limitar memória Zstd");
    std::vector<uint8_t> packed(1<<20),plain(1<<20);StreamHash sha;std::ofstream out;uint64_t total=0,written=0,part=0;size_t left=1;
    for(uint64_t pos=0;pos<f.stored;){
        auto n=std::min<uint64_t>(packed.size(),f.stored-pos);input.read(f.at+pos,packed.data(),size_t(n));pos+=n;ZSTD_inBuffer from{packed.data(),size_t(n),0};
        size_t produced=plain.size();
        while(from.pos<from.size || (produced==plain.size() && left)){
            auto previous=from.pos;ZSTD_outBuffer to{plain.data(),plain.size(),0};left=ZSTD_decompressStream(z.get(),&to,&from);
            if(ZSTD_isError(left))bad(std::string("Zstd: ")+ZSTD_getErrorName(left));
            if(to.pos>f.size-total)bad("Arquivo descomprimido excede o tamanho declarado");
            sha.update(plain.data(),to.pos);size_t at=0;
            while(at<to.pos){
                if(!out.is_open()){out.open(dir/part_name(part++),std::ios::binary|std::ios::trunc);if(!out)bad("Não foi possível criar parte do cache");written=0;}
                auto take=std::min<uint64_t>(to.pos-at,chunk-written);out.write(reinterpret_cast<const char*>(plain.data()+at),std::streamsize(take));if(!out)bad("Falha ao gravar cache no SD");
                at+=size_t(take);written+=take;if(written==chunk){out.close();if(!out)bad("Falha ao fechar parte do cache");}
            }
            total+=to.pos;produced=to.pos;report(total);
            if(!left&&(pos!=f.stored||from.pos!=from.size))bad("Dados adicionais após frame Zstd");
            if(!to.pos&&from.pos==previous){if(from.pos==from.size)break;bad("Descompressão sem progresso");}
        }
    }
    if(left||total!=f.size)bad("Arquivo Zstd incompleto");
    if(out.is_open()){out.close();if(!out)bad("Falha ao fechar cache");}
    if(sha.finish()!=f.hash)bad("SHA-256 do conteúdo não confere; cache incompleto preservado para diagnóstico");
    auto bytes=marker(f,chunk);std::ofstream complete(dir/"complete",std::ios::binary|std::ios::trunc);complete.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());complete.close();if(!complete)bad("Falha ao confirmar cache");
}
}
unsigned prepare_cache(const Input& input,const std::filesystem::path& root,const std::function<void(const std::string&,double)>& report,uint64_t threshold,uint64_t chunk){
    if(!chunk||chunk>0x40000000ull)bad("Tamanho de parte de cache inválido");
    archive_detail::Archive archive(input);auto files=archive.romfs();std::vector<std::pair<std::string,File>> chosen;std::set<std::string> seen;uint64_t needed=0;
    std::filesystem::create_directories(root);
    for(auto& [name,index]:files){auto f=archive.file(index);if(f.codec!=1||f.size<=threshold||!seen.insert(hex(f.hash)).second)continue;chosen.emplace_back(name,f);
        if(!ready(root/hex(f.hash),f,chunk)){if(UINT64_MAX-needed<f.size)bad("Cache excede limite de tamanho");needed+=f.size;}}
    // Discard only known files from our own interrupted staging directory. No recursive removal.
    for(auto& item:chosen){auto& f=item.second;auto stage=root/(hex(f.hash)+".partial");
        if(ready(root/hex(f.hash),f,chunk)||!std::filesystem::exists(stage))continue;
        std::filesystem::remove(stage/"complete");
        for(uint64_t off=0,i=0;off<f.size;off+=chunk,++i)std::filesystem::remove(stage/part_name(i));
    }
    uint64_t available;
#ifdef __SWITCH__
    s64 free_bytes=0;auto* sd=fsdevGetDeviceFileSystem("sdmc");
    if(!sd||R_FAILED(fsFsGetFreeSpace(sd,"/",&free_bytes))||free_bytes<0)bad("Não foi possível consultar o espaço livre no SD");
    available=uint64_t(free_bytes);
#else
    available=std::filesystem::space(root).available;
#endif
    if(needed && (needed>available||available-needed<(32ull<<20)))bad("Espaço insuficiente no SD para o cache descomprimido. O NUP original foi preservado.");
    for(size_t i=0;i<chosen.size();++i){auto& [name,f]=chosen[i];auto final=root/hex(f.hash);
        if(ready(final,f,chunk)){report("Cache disponível: "+name,double(i+1)/chosen.size());continue;}
        if(std::filesystem::exists(final))bad("Cache existente incompleto. Preserve os registros antes de remover a pasta indicada pelo hash.");
        auto stage=root/(hex(f.hash)+".partial");
        unpack(input,f,stage,chunk,[&](uint64_t n){report("Preparando cache "+std::to_string(i+1)+"/"+std::to_string(chosen.size())+": "+name+"\n"+std::to_string(n>>20)+" / "+std::to_string(f.size>>20)+" MiB",(double(i)+double(n)/double(f.size))/chosen.size());});
        std::filesystem::rename(stage,final);report("Cache verificado: "+name,double(i+1)/chosen.size());
    }
    return unsigned(chosen.size());
}
}
