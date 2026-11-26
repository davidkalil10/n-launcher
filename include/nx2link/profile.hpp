// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
namespace nx2link {
struct MemoryProfile {uint32_t game_mb{},loader_mb{};};
bool valid_game_memory(uint32_t);
bool valid_loader_memory(uint32_t);
uint32_t parse_memory(const std::string&,bool game);
uint32_t step_memory(uint32_t value,int direction,bool game);
std::string launch_arguments(const std::string& loader,const std::string& input,const std::string& menu,uint32_t loader_mb);
}
