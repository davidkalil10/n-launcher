// SPDX-License-Identifier: MIT
#include "nx2link/update.hpp"
#include "nx2link/input.hpp"
#include "nx2link/stream_hash.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#ifdef __SWITCH__
#include <switch.h>
#include <unistd.h>
#endif
namespace nx2link {
namespace {
[[noreturn]] void fail(const std::string& s){throw std::runtime_error(s);}
uint64_t number(const std::string& s){
    if(s.empty()||s.size()>18||s.find_first_not_of("0123456789")!=std::string::npos)fail("Número inválido no pacote de atualização");
    return std::stoull(s);
}
std::array<uint32_t,3> version(const std::string& s){
    if(s.empty()||s.size()>15)fail("Versão inválida");
    std::array<uint32_t,3> out{};size_t at=0;
    for(unsigned i=0;i<3;++i){auto end=s.find('.',at);if((i<2)==(end==std::string::npos))fail("Versão inválida");auto part=s.substr(at,end==std::string::npos?end:end-at);if(part.size()>1&&part[0]=='0')fail("Versão inválida");auto n=number(part);if(n>999999)fail("Versão inválida");out[i]=uint32_t(n);at=end+1;}
    return out;
}
uint64_t le(const uint8_t* p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(p[i])<<(8*i);return v;}
void range(uint64_t at,uint64_t n,uint64_t limit){if(at>limit||n>limit-at)fail("NRO de atualização incompleto");}
FILE* open_write(const std::filesystem::path& p){
#ifdef _WIN32
    return _wfopen(p.c_str(),L"wb");
#else
    return std::fopen(p.c_str(),"wb");
#endif
}
void verify_file(const std::filesystem::path& path,const UpdateManifest& m){
    Input input(path);if(input.parts().size()!=1||input.size()!=m.size)fail("Tamanho do arquivo gravado não confere");
    std::vector<uint8_t> buffer(256*1024);StreamHash hash;
    for(uint64_t at=0;at<input.size();){auto n=size_t(std::min<uint64_t>(buffer.size(),input.size()-at));input.read(at,buffer.data(),n);hash.update(buffer.data(),n);at+=n;}
    if(hash.finish()!=m.sha)fail("SHA-256 do arquivo no SD não confere");
}
std::string manifest_text(const UpdateManifest& m){
    const char* hex="0123456789abcdef";std::string sha;for(auto b:m.sha){sha+=hex[b>>4];sha+=hex[b&15];}
    return "schema=1\nversion="+m.version+"\nasset="+UpdateAsset+"\nsize="+std::to_string(m.size)+"\nsha256="+sha+"\n";
}
}
bool version_newer(const std::string& a,const std::string& b){return version(a)>version(b);}
UpdateManifest parse_update_manifest(const std::string& text){
    if(text.empty()||text.size()>4096||text.find('\0')!=std::string::npos)fail("Manifesto de atualização inválido");
    std::map<std::string,std::string> fields;std::istringstream lines(text);std::string line;
    while(std::getline(lines,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        auto at=line.find('=');if(at==std::string::npos||!fields.emplace(line.substr(0,at),line.substr(at+1)).second)fail("Campo duplicado ou inválido no manifesto");
    }
    if(fields.size()!=5||fields["schema"]!="1"||fields["asset"]!=UpdateAsset)fail("Formato de atualização não suportado");
    UpdateManifest m;m.version=fields["version"];version(m.version);m.size=number(fields["size"]);
    if(m.size<0x1000||m.size>(128ull<<20))fail("Tamanho da atualização fora do limite");
    const auto& sha=fields["sha256"];if(sha.size()!=64||sha.find_first_not_of("0123456789abcdef")!=std::string::npos)fail("SHA-256 inválido no manifesto");
    for(size_t i=0;i<32;++i)m.sha[i]=uint8_t(std::stoul(sha.substr(i*2,2),nullptr,16));
    return m;
}
std::string update_asset_url(const UpdateManifest& m){version(m.version);return std::string("https://github.com/")+UpdateRepository+"/releases/download/v"+m.version+"/"+UpdateAsset;}
UpdateManifest check_update(const UpdateFetch& fetch){std::string text;fetch(UpdateManifestURL,[&](const uint8_t* p,size_t n){if(n>4096-text.size())fail("Manifesto muito grande");text.append(reinterpret_cast<const char*>(p),n);},4096);return parse_update_manifest(text);}
void validate_update_nro(const std::filesystem::path& path,const UpdateManifest& m){
    Input input(path);if(input.parts().size()!=1||input.size()!=m.size)fail("Tamanho do NRO não confere");
    auto h=input.read(0,0x1000);if(std::memcmp(h.data()+16,"NRO0",4))fail("O arquivo baixado não é um NRO");
    auto mod=le(h.data()+4,4);range(mod,28,0x1000);if(std::memcmp(h.data()+mod,"MOD0",4))fail("Startup inválido");
    uint64_t at=mod+28;bool found=false;
    for(unsigned i=0;i<8;++i){range(at,12,0x1000);if(!std::memcmp(h.data()+at,"OUN0",4)){if(le(h.data()+at+4,4)!=2||le(h.data()+at+8,4)!=1)fail("A atualização não é para Switch 2");found=true;break;}if(std::memcmp(h.data()+at,"LNY",3))fail("Marca de plataforma ausente");at+=12;}
    if(!found)fail("Marca Switch 2 ausente");
    auto image=le(h.data()+24,4);range(image,0x38,input.size());
    for(unsigned off:{0x20u,0x28u,0x30u})range(le(h.data()+off,4),le(h.data()+off+4,4),image);
    auto aset=input.read(image,0x38);if(std::memcmp(aset.data(),"ASET",4))fail("Metadados da atualização ausentes");
    auto icon=le(aset.data()+8,8),iconsize=le(aset.data()+16,8),nacp=le(aset.data()+24,8),nacpsize=le(aset.data()+32,8);
    if(iconsize<64||nacpsize!=0x4000)fail("Metadados da atualização inválidos");
    range(icon,iconsize,input.size()-image);range(nacp,nacpsize,input.size()-image);
    auto jpeg=input.read(image+icon,3);if(jpeg[0]!=255||jpeg[1]!=216||jpeg[2]!=255)fail("Ícone da atualização inválido");
    auto meta=input.read(image+nacp,0x4000);const char title[]="N Launcher";
    if(std::memcmp(meta.data(),title,sizeof title))fail("O pacote não é do N Launcher");
    char current[17]{};std::memcpy(current,meta.data()+0x3060,16);if(std::string(current)!=m.version)fail("A versão do pacote não corresponde ao manifesto");
}
void install_update(const UpdateManifest& m,const std::string& current,const std::filesystem::path& executable,const UpdateFetch& fetch,const UpdateProgress& progress){
    if(m.size<0x1000||m.size>(128ull<<20))fail("Tamanho de atualização inválido");
    if(!version_newer(m.version,current))fail("A atualização não é mais recente");
    if(!std::filesystem::is_regular_file(executable)||executable.extension()!=".nro")fail("Abra um NRO salvo no SD para atualizar. O Netloader não fornece um arquivo permanente.");
    auto staging=executable;staging+=".update-part";auto backup=executable;backup+=".previous";
    auto state=executable;state+=".update-state";
    if(std::filesystem::exists(backup))fail("Existe uma versão anterior aguardando confirmação. Reabra o aplicativo para concluir a atualização anterior.");
#ifdef __SWITCH__
    s64 available=0;auto* sd=fsdevGetDeviceFileSystem("sdmc");if(!sd||R_FAILED(fsFsGetFreeSpace(sd,"/",&available))||available<0||uint64_t(available)<m.size+(4u<<20))fail("Espaço insuficiente no SD para a atualização");
#else
    if(std::filesystem::space(executable.parent_path()).available<m.size+(4u<<20))fail("Espaço insuficiente para atualizar");
#endif
    uint64_t done=0;StreamHash hash;FILE* file=nullptr;
    try{
        progress("Baixando N Launcher "+m.version,0);file=open_write(staging);if(!file)fail("Não foi possível criar o download no SD");
        fetch(update_asset_url(m),[&](const uint8_t* bytes,size_t n){
            if(n>m.size-done)fail("Download maior que o tamanho esperado");
            if(std::fwrite(bytes,1,n,file)!=n)fail("Falha ao gravar a atualização");
            hash.update(bytes,n);done+=n;progress("Baixando "+m.version+" · "+std::to_string(done>>10)+" / "+std::to_string(m.size>>10)+" KiB",double(done)/double(m.size));
        },m.size);
        if(std::fflush(file))fail("Falha ao finalizar o download");
#ifdef __SWITCH__
        if(fsync(fileno(file)))fail("Falha ao sincronizar a atualização no SD");
#endif
        auto* closed=file;file=nullptr;if(std::fclose(closed))fail("Falha ao fechar o download");
        if(done!=m.size||hash.finish()!=m.sha)fail("A integridade da atualização não confere. A versão atual foi preservada.");
        progress("Verificando o aplicativo baixado",1);verify_file(staging,m);validate_update_nro(staging,m);
        progress("Instalando atualização verificada",1); // cancellation still allowed before any replacement
        {
            auto text=manifest_text(m);auto* record=open_write(state);if(!record)fail("Não foi possível preservar o estado da atualização");
            bool ok=std::fwrite(text.data(),1,text.size(),record)==text.size()&&std::fflush(record)==0;
#ifdef __SWITCH__
            if(ok)ok=fsync(fileno(record))==0;
#endif
            if(std::fclose(record))ok=false;
            if(!ok)fail("Falha ao preservar o estado da atualização");
        }
        std::filesystem::rename(executable,backup);
        try{std::filesystem::rename(staging,executable);}catch(...){std::filesystem::rename(backup,executable);throw;}
    }catch(...){if(file)std::fclose(file);std::error_code ec;std::filesystem::remove(staging,ec);if(!std::filesystem::exists(backup,ec))std::filesystem::remove(state,ec);throw;}
}
bool confirm_update(const std::filesystem::path& executable,const std::string& current){
    auto state=executable;state+=".update-state";if(!std::filesystem::exists(state))return false;
    Input record(state);if(record.size()>4096)fail("Estado de atualização inválido");auto bytes=record.read(0,size_t(record.size()));auto m=parse_update_manifest(std::string(bytes.begin(),bytes.end()));
    if(m.version!=current)return false; // the old process cannot acknowledge a newly installed version
    verify_file(executable,m);validate_update_nro(executable,m);
    auto backup=executable;backup+=".previous";std::filesystem::remove(backup);std::filesystem::remove(state);return true;
}
}
