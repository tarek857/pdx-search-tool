#pragma once

#include "pdxinfo/Xml.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace pdxinfo::detail {

class OdxNodeReader {
public:
    using Node = xml::Node;

    std::vector<const Node*> collectAll(const Node& node) const;
    std::vector<const Node*> collectByLocalName(const Node& node, const std::set<std::string>& names) const;

    std::string attr(const Node& node, const std::string& name) const;
    std::string nodeId(const Node& node) const;
    std::string refId(const Node& node) const;
    std::string shortNameOf(const Node& node) const;

    std::optional<std::uint64_t> parseInteger(std::string text) const;
    std::string upper(std::string value) const;

private:
    void collectAllInto(const Node& node, std::vector<const Node*>& out) const;
    void collectByLocalNameInto(const Node& node,
        const std::set<std::string>& names,
        std::vector<const Node*>& out) const;
};

}
