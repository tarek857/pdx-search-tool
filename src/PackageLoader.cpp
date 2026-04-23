#include "pdxinfo/PackageLoader.hpp"

#include "pdxinfo/OdxParser.hpp"
#include "pdxinfo/ZipArchive.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace pdxinfo {
namespace {

bool hasOdxExtension(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".odx" || ext == ".xml";
}

bool hasPdxExtension(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".pdx" || ext == ".zip";
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void appendDocument(DiagnosticDatabase& database,
    const OdxParser& parser,
    const std::string& sourceName,
    const std::string& content) {
    database.documents.push_back(parser.parse(sourceName, content));
}

}

DiagnosticDatabase PackageLoader::load(const std::filesystem::path& input) const {
    DiagnosticDatabase database;
    OdxParser parser;

    for (const auto& document : loadRawDocuments(input)) {
        appendDocument(database, parser, document.sourceName, document.content);
    }

    if (database.documents.empty()) {
        throw std::runtime_error("No ODX/XML documents found in input");
    }

    return database;
}

std::vector<RawDocument> PackageLoader::loadRawDocuments(const std::filesystem::path& input) const {
    if (!std::filesystem::exists(input)) {
        throw std::runtime_error("Input does not exist: " + input.string());
    }

    std::vector<RawDocument> documents;

    if (std::filesystem::is_directory(input)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(input)) {
            if (!entry.is_regular_file() || !hasOdxExtension(entry.path())) {
                continue;
            }
            documents.push_back(RawDocument{entry.path().string(), readTextFile(entry.path())});
        }
    } else if (hasPdxExtension(input)) {
        ZipArchive archive;
        for (const auto& entry : archive.readFiles(input)) {
            const std::filesystem::path entryPath(entry.name);
            if (!hasOdxExtension(entryPath)) {
                continue;
            }
            const std::string text(entry.data.begin(), entry.data.end());
            documents.push_back(RawDocument{entry.name, text});
        }
    } else if (hasOdxExtension(input)) {
        documents.push_back(RawDocument{input.string(), readTextFile(input)});
    } else {
        throw std::runtime_error("Unsupported input type: " + input.string());
    }

    if (documents.empty()) {
        throw std::runtime_error("No ODX/XML documents found in input");
    }

    return documents;
}

}
