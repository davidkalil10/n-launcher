// SPDX-License-Identifier: MIT
#pragma once
#include "update.hpp"
namespace nx2link {
struct HttpsURL {std::string host,path;};
HttpsURL parse_update_url(const std::string&);
struct HttpResponse {int status{};std::string location;uint64_t bytes{};};
using HttpRead=std::function<size_t(uint8_t*,size_t)>;
HttpResponse read_update_response(const HttpRead&,const UpdateSink&,uint64_t limit);
// Console implementation uses native TLS with certificate/host verification; never HTTP fallback.
void fetch_update_https(const std::string&,const UpdateSink&,uint64_t limit);
}
