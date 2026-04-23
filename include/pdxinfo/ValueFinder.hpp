#pragma once

#include "pdxinfo/PackageLoader.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace pdxinfo {

struct ValueQuery {
    std::string text;
};

void writeValueFindings(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries);

void writeInteractiveValueFindings(std::istream& in,
    std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries);

}
