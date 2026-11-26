// SPDX-License-Identifier: GPL-2.0-or-later
// Native TLS approach adapted from nx2-flow menu/source/main_switch.cpp (Olmectron).
// Streaming body decoder and strict updater policy are in the portable MIT core.
#include "nx2link/http.hpp"
#include <switch.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>
namespace nx2link {
namespace {
struct Network {
    bool nifm{},sockets{},ssl{};
    Network(){
        nifm=R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
        sockets=R_SUCCEEDED(socketInitializeDefault());ssl=R_SUCCEEDED(sslInitialize(3));
    }
    ~Network(){if(ssl)sslExit();if(sockets)socketExit();if(nifm)nifmExit();}
};
struct Connection {
    int socket{-1};SslContext context{};SslConnection connection{};bool has_context{},has_connection{};
    ~Connection(){if(has_connection)sslConnectionClose(&connection);if(has_context)sslContextClose(&context);if(socket>=0)close(socket);}
    void open(const std::string& host){
        addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;addrinfo* entries=nullptr;
        if(getaddrinfo(host.c_str(),"443",&hints,&entries)||!entries)throw std::runtime_error("Não foi possível localizar o GitHub. Confira a conexão de rede.");
        for(auto* p=entries;p;p=p->ai_next){
            socket=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(socket<0)continue;
            int flags=fcntl(socket,F_GETFL,0);bool connected=false;
            if(flags>=0&&fcntl(socket,F_SETFL,flags|O_NONBLOCK)==0){
                auto rc=connect(socket,p->ai_addr,p->ai_addrlen);if(!rc)connected=true;
                else if(errno==EINPROGRESS){pollfd wait{socket,POLLOUT,0};int error=0;socklen_t size=sizeof error;connected=poll(&wait,1,10000)>0&&getsockopt(socket,SOL_SOCKET,SO_ERROR,&error,&size)==0&&!error;}
                fcntl(socket,F_SETFL,flags);
            }
            if(connected)break;close(socket);socket=-1;
        }
        freeaddrinfo(entries);if(socket<0)throw std::runtime_error("O GitHub não respondeu. Tente novamente mais tarde.");
        timeval timeout{15,0};setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
        if(R_FAILED(sslCreateContext(&context,SslVersion_TlsV12)))throw std::runtime_error("Não foi possível iniciar o TLS");has_context=true;
        if(R_FAILED(sslContextCreateConnection(&context,&connection)))throw std::runtime_error("Não foi possível criar a conexão segura");has_connection=true;
        int taken=-1;
        if(R_FAILED(sslConnectionSetOption(&connection,SslOptionType_DoNotCloseSocket,1))||
           R_FAILED(sslConnectionSetVerifyOption(&connection,SslVerifyOption_PeerCa|SslVerifyOption_HostName|SslVerifyOption_DateCheck))||
           R_FAILED(sslConnectionSetSocketDescriptor(&connection,socket,&taken))||
           R_FAILED(sslConnectionSetHostName(&connection,host.c_str(),u32(host.size())))||
           R_FAILED(sslConnectionDoHandshake(&connection,nullptr,nullptr,nullptr,0)))throw std::runtime_error("Falha na conexão segura. Confira a rede e a data/hora do console.");
    }
    void send(const std::string& message){size_t at=0;while(at<message.size()){u32 n=0;if(R_FAILED(sslConnectionWrite(&connection,message.data()+at,u32(message.size()-at),&n))||!n)throw std::runtime_error("Falha ao enviar a solicitação ao GitHub");at+=n;}}
    size_t read(uint8_t* bytes,size_t size){u32 n=0;if(R_FAILED(sslConnectionRead(&connection,bytes,u32(size),&n)))throw std::runtime_error("A conexão de download foi interrompida");return n;}
};
}
void fetch_update_https(const std::string& source,const UpdateSink& sink,uint64_t limit){
    Network network;if(!network.nifm||!network.sockets||!network.ssl)throw std::runtime_error("Não foi possível iniciar a rede para atualizar");
    std::string url=source;
    for(unsigned hop=0;hop<6;++hop){
        auto parsed=parse_update_url(url);Connection line;line.open(parsed.host);
        line.send("GET "+parsed.path+" HTTP/1.1\r\nHost: "+parsed.host+"\r\nUser-Agent: N-Launcher-Updater/1\r\nAccept: application/octet-stream\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
        auto result=read_update_response([&](uint8_t* p,size_t n){return line.read(p,n);},sink,limit);
        if(result.status==200)return;
        if(!result.location.empty()){url=result.location[0]=='/'?"https://"+parsed.host+result.location:result.location;continue;}
        if(result.status==404)throw std::runtime_error("Ainda não há uma atualização pública disponível no GitHub.");
        throw std::runtime_error("O GitHub respondeu com erro "+std::to_string(result.status));
    }
    throw std::runtime_error("Muitos redirecionamentos no download");
}
}
