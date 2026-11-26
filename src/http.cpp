// SPDX-License-Identifier: MIT
#include "nx2link/http.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
namespace nx2link {
namespace {
[[noreturn]] void fail(const char* s){throw std::runtime_error(s);}
std::string lower(std::string s){for(char& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \t");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t");return s.substr(a,b-a+1);}
uint64_t decimal(const std::string& s){if(s.empty()||s.size()>18||s.find_first_not_of("0123456789")!=std::string::npos)fail("Tamanho HTTP inválido");return std::stoull(s);}
class Reader {
    const HttpRead& read;std::array<uint8_t,16384> buffer{};size_t pos{},size{};
public:
    explicit Reader(const HttpRead& f):read(f){}
    bool fill(){if(pos<size)return true;size=read(buffer.data(),buffer.size());pos=0;if(size>buffer.size())fail("Resposta de rede inválida");return size!=0;}
    std::string line(size_t limit){std::string s;for(;;){if(!fill())fail("Resposta HTTP interrompida");char c=char(buffer[pos++]);if(c=='\n'){if(s.empty()||s.back()!='\r')fail("Cabeçalho HTTP inválido");s.pop_back();return s;}s+=c;if(s.size()>limit)fail("Linha HTTP muito grande");}}
    uint64_t body(uint64_t n,const UpdateSink& sink){uint64_t done=0;while(done<n&&fill()){auto count=size_t(std::min<uint64_t>(size-pos,n-done));sink(buffer.data()+pos,count);pos+=count;done+=count;}return done;}
};
}
HttpsURL parse_update_url(const std::string& url){
    if(url.size()>8192||url.rfind("https://",0)!=0||url.find_first_of("\r\n\t ")!=std::string::npos)fail("URL de atualização inválida; HTTPS obrigatório");
    for(unsigned char c:url)if(c<0x21||c==0x7f)fail("URL de atualização inválida");
    auto at=url.find('/',8);auto host=url.substr(8,at==std::string::npos?at:at-8);
    if(host!="github.com"&&host!="release-assets.githubusercontent.com"&&host!="objects.githubusercontent.com")fail("Redirecionamento para servidor não autorizado");
    return {host,at==std::string::npos?"/":url.substr(at)};
}
HttpResponse read_update_response(const HttpRead& read,const UpdateSink& sink,uint64_t limit){
    Reader in(read);auto statusline=in.line(8192);std::istringstream status(statusline);std::string protocol;HttpResponse result;status>>protocol>>result.status;
    if((protocol!="HTTP/1.1"&&protocol!="HTTP/1.0")||result.status<100||result.status>599)fail("Resposta HTTP inválida");
    std::map<std::string,std::string> headers;size_t total=statusline.size();
    for(;;){auto line=in.line(8192);total+=line.size()+2;if(total>65536)fail("Cabeçalho HTTP muito grande");if(line.empty())break;auto at=line.find(':');if(at==std::string::npos||!at)fail("Cabeçalho HTTP inválido");auto key=lower(line.substr(0,at)),value=trim(line.substr(at+1));
        if(key=="content-length"||key=="transfer-encoding"||key=="location"||key=="content-encoding")if(!headers.emplace(key,value).second)fail("Cabeçalho HTTP duplicado");
    }
    if(result.status==301||result.status==302||result.status==303||result.status==307||result.status==308){result.location=headers["location"];if(result.location.empty())fail("Redirecionamento sem destino");return result;}
    if(result.status!=200)return result;
    if(headers.count("content-encoding")&&lower(headers["content-encoding"])!="identity")fail("Compressão HTTP não suportada");
    if(headers.count("transfer-encoding")&&headers.count("content-length"))fail("Comprimento HTTP ambíguo");
    auto consume=[&](const uint8_t* bytes,size_t n){if(n>limit-result.bytes)fail("Download excede o limite permitido");sink(bytes,n);result.bytes+=n;};
    if(headers.count("transfer-encoding")){
        if(lower(headers["transfer-encoding"])!="chunked")fail("Codificação HTTP não suportada");
        for(;;){auto line=in.line(128);auto semicolon=line.find(';');auto token=line.substr(0,semicolon);if(token.empty()||token.size()>16||token.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)fail("Bloco HTTP inválido");
            auto n=std::stoull(token,nullptr,16);if(n>limit-result.bytes)fail("Bloco HTTP excede o limite");
            if(!n){size_t trailer=0;for(;;){auto t=in.line(8192);trailer+=t.size()+2;if(trailer>16384)fail("Rodapé HTTP muito grande");if(t.empty())break;}break;}
            if(in.body(n,consume)!=n||!in.line(2).empty())fail("Bloco HTTP incompleto");
        }
    }else if(headers.count("content-length")){
        auto n=decimal(headers["content-length"]);if(n>limit)fail("Download muito grande");if(in.body(n,consume)!=n)fail("Download interrompido");
    }else{
        in.body(limit,consume);if(in.fill())fail("Download excede o limite");
    }
    return result;
}
}
