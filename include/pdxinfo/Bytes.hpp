#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pdxinfo {

using Bytes = std::vector<std::uint8_t>;

std::string toHex(std::uint64_t value, int width = 0);

}
