#include "pdxinfo/Xml.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace pdxinfo::xml {
namespace {

void collectText(const Node& node, std::string& out) {
    if (!node.text.empty()) {
        if (!out.empty()) {
            out += " ";
        }
        out += node.text;
    }
    for (const auto& child : node.children) {
        collectText(*child, out);
    }
}

std::unique_ptr<Node> convertElement(const pugi::xml_node& element) {
    auto node = std::make_unique<Node>();
    node->name = element.name();

    for (const auto& attribute : element.attributes()) {
        node->attributes[attribute.name()] = attribute.value();
    }

    for (const auto& child : element.children()) {
        if (child.type() == pugi::node_element) {
            node->children.push_back(convertElement(child));
        } else if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
            node->text += child.value();
        }
    }

    node->text = trim(std::move(node->text));
    return node;
}

}

const Node* Node::firstChildLocal(const std::string& wantedLocalName) const {
    for (const auto& child : children) {
        if (localName(child->name) == wantedLocalName) {
            return child.get();
        }
    }
    return nullptr;
}

std::vector<const Node*> Node::childrenLocal(const std::string& wantedLocalName) const {
    std::vector<const Node*> result;
    for (const auto& child : children) {
        if (localName(child->name) == wantedLocalName) {
            result.push_back(child.get());
        }
    }
    return result;
}

std::string Node::childTextLocal(const std::string& wantedLocalName) const {
    if (const auto* child = firstChildLocal(wantedLocalName)) {
        std::string text;
        collectText(*child, text);
        return trim(text);
    }
    return {};
}

std::unique_ptr<Node> parse(const std::string& input) {
    pugi::xml_document document;
    const auto result = document.load_buffer(input.data(), input.size());
    if (!result) {
        throw std::runtime_error(std::string("Failed to parse XML: ") + result.description());
    }

    auto root = std::make_unique<Node>();
    root->name = "#document";

    for (const auto& child : document.children()) {
        if (child.type() == pugi::node_element) {
            root->children.push_back(convertElement(child));
        }
    }

    return root;
}

std::string localName(const std::string& qualifiedName) {
    const auto pos = qualifiedName.find(':');
    if (pos == std::string::npos) {
        return qualifiedName;
    }
    return qualifiedName.substr(pos + 1);
}

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }).base(), value.end());
    return value;
}

}
