// SPDX-License-Identifier: MIT
#pragma once
#include "input.hpp"
#include <functional>
#include <string>
namespace nx2link {
// Called on a host-owned thread, never a guest thread. Throws on cancel/write/hash/space failure.
// Returns the number of large compressed files ready on SD. Original NUPs and saves are read-only.
unsigned prepare_cache(const Input&,const std::filesystem::path& root,
    const std::function<void(const std::string&,double)>& report,uint64_t threshold=64ull<<20,uint64_t chunk_size=1ull<<30);
}
