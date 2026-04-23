#include "pdxinfo/ZipArchive.hpp"

#include <fstream>
#include <stdexcept>
#include <zlib.h>

namespace pdxinfo {
namespace {

std::uint16_t read16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Invalid ZIP structure");
    }
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Invalid ZIP structure");
    }
    return static_cast<std::uint32_t>(data[offset] |
        (data[offset + 1] << 8) |
        (data[offset + 2] << 16) |
        (data[offset + 3] << 24));
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open ZIP file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> inflateRaw(const std::vector<std::uint8_t>& compressed, std::size_t expectedSize) {
    std::vector<std::uint8_t> output(expectedSize);

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to inflate ZIP entry");
    }

    return output;
}

}

std::vector<ZipEntry> ZipArchive::readFiles(const std::filesystem::path& path) const {
    const auto data = readBinaryFile(path);
    if (data.size() < 22) {
        throw std::runtime_error("File is too small to be a ZIP/PDX package");
    }

    std::size_t eocd = std::string::npos;
    const std::size_t searchStart = data.size() > 66000 ? data.size() - 66000 : 0;
    for (std::size_t i = data.size() - 22; i + 1 > searchStart; --i) {
        if (read32(data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
        if (i == 0) {
            break;
        }
    }
    if (eocd == std::string::npos) {
        throw std::runtime_error("Cannot find ZIP central directory");
    }

    const auto entryCount = read16(data, eocd + 10);
    const auto centralDirectoryOffset = read32(data, eocd + 16);

    std::vector<ZipEntry> entries;
    std::size_t offset = centralDirectoryOffset;
    for (std::uint16_t i = 0; i < entryCount; ++i) {
        if (read32(data, offset) != 0x02014b50) {
            throw std::runtime_error("Invalid ZIP central directory entry");
        }

        const auto method = read16(data, offset + 10);
        const auto compressedSize = read32(data, offset + 20);
        const auto uncompressedSize = read32(data, offset + 24);
        const auto fileNameLength = read16(data, offset + 28);
        const auto extraLength = read16(data, offset + 30);
        const auto commentLength = read16(data, offset + 32);
        const auto localHeaderOffset = read32(data, offset + 42);

        const std::string name(reinterpret_cast<const char*>(data.data() + offset + 46), fileNameLength);
        offset += 46 + fileNameLength + extraLength + commentLength;

        if (name.empty() || name.back() == '/') {
            continue;
        }

        if (read32(data, localHeaderOffset) != 0x04034b50) {
            throw std::runtime_error("Invalid ZIP local header");
        }
        const auto localNameLength = read16(data, localHeaderOffset + 26);
        const auto localExtraLength = read16(data, localHeaderOffset + 28);
        const auto dataOffset = localHeaderOffset + 30 + localNameLength + localExtraLength;

        if (dataOffset + compressedSize > data.size()) {
            throw std::runtime_error("ZIP entry exceeds file size");
        }

        std::vector<std::uint8_t> compressed(data.begin() + dataOffset, data.begin() + dataOffset + compressedSize);
        std::vector<std::uint8_t> payload;
        if (method == 0) {
            payload = std::move(compressed);
        } else if (method == 8) {
            payload = inflateRaw(compressed, uncompressedSize);
        } else {
            continue;
        }

        entries.push_back(ZipEntry{name, std::move(payload)});
    }

    return entries;
}

}

