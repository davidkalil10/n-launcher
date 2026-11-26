#pragma once
#include <cstdint>
using Result=uint32_t;
#define R_SUCCEEDED(rc) ((rc)==0)
struct SslConnection{};
int socketSslConnectionSetSocketDescriptor(SslConnection*,int);
Result socketGetLastResult();
