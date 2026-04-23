#include "pdxinfo/Bytes.hpp"

#include <iomanip>
#include <sstream>

namespace pdxinfo {

std::string toHex(std::uint64_t value, int width) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setfill('0');
    if (width > 0) {
        out << std::setw(width);
    }
    out << value;
    return out.str();
}

}
