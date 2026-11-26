// SPDX-License-Identifier: MIT
#include "nx2link/tls_socket.hpp"
#include <cassert>
#include <iostream>
static int supplied,returned,error_value;
int socketSslConnectionSetSocketDescriptor(SslConnection*,int fd){supplied=fd;errno=error_value;return returned;}
Result socketGetLastResult(){return 0x1234;}
int main(){
    SslConnection connection;returned=-1;error_value=ENOENT;
    assert(nx2link::bind_tls_socket(connection,42,"github.com")==-1&&supplied==42);
    returned=73;error_value=0;assert(nx2link::bind_tls_socket(connection,42,"github.com")==73);
    returned=-1;error_value=EIO;bool failed=false;
    try{nx2link::bind_tls_socket(connection,42,"github.com");}catch(const std::runtime_error& e){auto message=std::string(e.what());failed=message.find("associar socket")!=std::string::npos&&message.find("github.com")!=std::string::npos&&message.find("0x00001234")!=std::string::npos;}
    assert(failed);error_value=EBADF;failed=false;try{nx2link::bind_tls_socket(connection,42,"github.com");}catch(const std::runtime_error&){failed=true;}assert(failed);
    nx2link::require_tls(0,"negociar TLS","github.com");failed=false;
    try{nx2link::require_tls(0xCAFE,"negociar TLS","github.com");}catch(const std::runtime_error& e){auto message=std::string(e.what());failed=message.find("negociar TLS")!=std::string::npos&&message.find("0x0000CAFE")!=std::string::npos;}
    assert(failed);std::cout<<"PASS: native TLS descriptor wrapper, ENOENT success, returned handle and stage diagnostics\n";
}
