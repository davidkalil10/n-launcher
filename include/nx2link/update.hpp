// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
namespace nx2link {
inline constexpr const char* UpdateRepository="davidkalil10/nx2-link-enhanced";
inline constexpr const char* UpdateAsset="n-launcher.nro";
inline constexpr const char* UpdateManifestURL="https://github.com/davidkalil10/nx2-link-enhanced/releases/latest/download/update.ini";
struct UpdateManifest {std::string version;uint64_t size{};std::array<uint8_t,32> sha{};};
UpdateManifest parse_update_manifest(const std::string&);
bool version_newer(const std::string& candidate,const std::string& current);
std::string update_asset_url(const UpdateManifest&);
using UpdateSink=std::function<void(const uint8_t*,size_t)>;
using UpdateFetch=std::function<void(const std::string&,const UpdateSink&,uint64_t)>;
using UpdateProgress=std::function<void(const std::string&,double)>;
UpdateManifest check_update(const UpdateFetch&);
void validate_update_nro(const std::filesystem::path&,const UpdateManifest&);
void install_update(const UpdateManifest&,const std::string& current,const std::filesystem::path& executable,
                    const UpdateFetch&,const UpdateProgress&);
// Called only by the newly started app; removes its own backup after matching state, version and hash.
bool confirm_update(const std::filesystem::path& executable,const std::string& current);
}
