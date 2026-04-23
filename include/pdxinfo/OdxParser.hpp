#pragma once

#include "pdxinfo/OdxDataModel.hpp"

#include <string>

namespace pdxinfo
{
    class OdxParser
    {
    public:
        OdxDocument parse(const std::string &sourceName, const std::string &xmlText) const;
    };
}
