#pragma once
#include "nx2link/input.hpp"
#include <functional>
#include <string>
namespace nx2link {
struct PreparedModule {
    std::vector<uint8_t> image;
    uint64_t end;
    uint32_t text_size,ro_offset,data_offset,mod0,dynamic,module_object;
    std::vector<std::pair<uint32_t,uint32_t>> svcs;
};
PreparedModule prepare_nso(const std::vector<uint8_t>& raw);
// One module at a time; no persistent dump/cache, mapping or game entrypoint.
unsigned prepare_program(const Input&,const std::function<void(const std::string&)>& report);
}
