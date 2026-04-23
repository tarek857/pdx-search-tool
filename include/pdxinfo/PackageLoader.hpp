#pragma once

#include "pdxinfo/OdxDataModel.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace pdxinfo {

struct RawDocument {
    std::string sourceName;
    std::string content;
};

class PackageLoader {
public:
    DiagnosticDatabase load(const std::filesystem::path& input) const;
    std::vector<RawDocument> loadRawDocuments(const std::filesystem::path& input) const;
};

}
