#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace pdxinfo::xml {

struct Node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string text;
    std::vector<std::unique_ptr<Node>> children;

    const Node* firstChildLocal(const std::string& localName) const;
    std::vector<const Node*> childrenLocal(const std::string& localName) const;
    std::string childTextLocal(const std::string& localName) const;
};

std::unique_ptr<Node> parse(const std::string& input);
std::string localName(const std::string& qualifiedName);
std::string trim(std::string value);

}

