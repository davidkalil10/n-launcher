// SPDX-License-Identifier: MIT
#pragma once
#include <switch.h>
#include <cerrno>
#include <cstdio>
#include <stdexcept>
#include <string>
namespace nx2link {
inline void require_tls(Result rc,const char* stage,const std::string& host){
    if(R_SUCCEEDED(rc))return;
    char code[40];std::snprintf(code,sizeof code," [TLS 0x%08X]",unsigned(rc));
    throw std::runtime_error(std::string("Conexão segura: ")+stage+" em "+host+code);
}
inline int bind_tls_socket(SslConnection& connection,int socket,const std::string& host){
    // POSIX/newlib handles differ from BSD service descriptors. The SDK wrapper
    // performs that conversion. DoNotCloseSocket returns -1/ENOENT on success.
    errno=0;
    const int duplicate=socketSslConnectionSetSocketDescriptor(&connection,socket);
    const int error=errno;
    if(duplicate<0&&error!=ENOENT){
        char code[64];std::snprintf(code,sizeof code," [errno %d; rede 0x%08X]",error,unsigned(socketGetLastResult()));
        throw std::runtime_error("Conexão segura: associar socket em "+host+code);
    }
    return duplicate;
}
}
