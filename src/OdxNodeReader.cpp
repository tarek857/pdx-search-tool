#include "pdxinfo/OdxNodeReader.hpp"

#include <algorithm>
#include <cctype>

namespace pdxinfo::detail {

std::vector<const OdxNodeReader::Node*> OdxNodeReader::collectAll(const Node& node) const {
    std::vector<const Node*> result;
    collectAllInto(node, result);
    return result;
}

std::vector<const OdxNodeReader::Node*> OdxNodeReader::collectByLocalName(
    const Node& node,
    const std::set<std::string>& names) const {
    std::vector<const Node*> result;
    collectByLocalNameInto(node, names, result);
    return result;
}

std::string OdxNodeReader::attr(const Node& node, const std::string& name) const {
    const auto it = node.attributes.find(name);
    if (it != node.attributes.end()) {
        return it->second;
    }
    return {};
}

std::string OdxNodeReader::nodeId(const Node& node) const {
    for (const auto& candidate : {"ID", "OID", "UUID"}) {
        const auto value = attr(node, candidate);
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string OdxNodeReader::refId(const Node& node) const {
    for (const auto& candidate : {"ID-REF", "DOCREF", "OID-REF"}) {
        const auto value = attr(node, candidate);
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string OdxNodeReader::shortNameOf(const Node& node) const {
    auto name = node.childTextLocal("SHORT-NAME");
    if (name.empty()) {
        name = attr(node, "SHORT-NAME");
    }
    if (name.empty()) {
        name = nodeId(node);
    }
    return name;
}

std::optional<std::uint64_t> OdxNodeReader::parseInteger(std::string text) const {
    text = xml::trim(std::move(text));
    if (text.empty()) {
        return std::nullopt;
    }

    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    }

    try {
        std::size_t processed = 0;
        const auto value = std::stoull(text, &processed, base);
        if (processed == text.size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::string OdxNodeReader::upper(std::string value) const {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

void OdxNodeReader::collectAllInto(const Node& node, std::vector<const Node*>& out) const {
    out.push_back(&node);
    for (const auto& child : node.children) {
        collectAllInto(*child, out);
    }
}

void OdxNodeReader::collectByLocalNameInto(
    const Node& node,
    const std::set<std::string>& names,
    std::vector<const Node*>& out) const {
    if (names.count(xml::localName(node.name)) > 0) {
        out.push_back(&node);
    }
    for (const auto& child : node.children) {
        collectByLocalNameInto(*child, names, out);
    }
}

}
