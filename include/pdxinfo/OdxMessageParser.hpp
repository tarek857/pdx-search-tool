#pragma once

#include "OdxNodeReader.hpp"

#include "pdxinfo/OdxDataModel.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace pdxinfo::detail {

class OdxMessageParser {
public:
    explicit OdxMessageParser(const OdxNodeReader& reader);

    Parameter parseParameter(const xml::Node& node) const;
    Message parseMessage(const xml::Node& node) const;
    std::optional<std::uint64_t> findSid(const std::vector<Message>& messages) const;

private:
    const OdxNodeReader& reader_;
};

}
