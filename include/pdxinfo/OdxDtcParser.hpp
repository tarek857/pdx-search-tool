#pragma once

#include "OdxNodeReader.hpp"

#include "pdxinfo/OdxDataModel.hpp"

namespace pdxinfo::detail {

class OdxDtcParser {
public:
    explicit OdxDtcParser(const OdxNodeReader& reader);

    Dtc parseDtc(const xml::Node& node) const;

private:
    const OdxNodeReader& reader_;
};

}
