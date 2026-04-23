#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pdxinfo {

struct ZipEntry {
    std::string name;
    std::vector<std::uint8_t> data;
};

class ZipArchive {
public:
    std::vector<ZipEntry> readFiles(const std::filesystem::path& path) const;
};

}

