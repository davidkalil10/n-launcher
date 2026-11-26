#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace nx2link {
struct Part { std::filesystem::path path; uint64_t start, size; };
class Input {
public:
    explicit Input(const std::filesystem::path& source);
    uint64_t size() const { return size_; }
    const std::vector<Part>& parts() const { return parts_; }
    void read(uint64_t offset, void* out, size_t bytes) const;
    std::vector<uint8_t> read(uint64_t offset, size_t bytes) const;
private:
    std::vector<Part> parts_;
    uint64_t size_ = 0;
};
struct Package { uint64_t title; uint32_t version; uint8_t kind; };
struct Inventory {
    uint64_t directories, files;
    std::vector<Package> packages;
};
// Inventory only: verifies the TOC's SHA-256, not every payload's digest.
Inventory inspect(const Input& input);
}
