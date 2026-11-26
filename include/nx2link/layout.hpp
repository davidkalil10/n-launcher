// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
namespace nx2link {
struct Migration {std::string executable;bool moved{};};
// Move directory entries, never copy large payloads or overwrite existing data.
// Refuse conflicting datasets before moving anything.
Migration migrate_layout(const std::filesystem::path& sd_root,const std::string& executable);
}
