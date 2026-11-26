// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace nlauncher {
// Append/flush/close each stage: survives a later abort and is never a game/save write.
void launch_trace(const char* phase,const std::string& detail={});
bool begin_launch_trace(const std::string& path);
}
