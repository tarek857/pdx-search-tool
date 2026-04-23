#pragma once

#include "OdxMessageParser.hpp"
#include "OdxNodeReader.hpp"

#include "pdxinfo/OdxDataModel.hpp"

#include <map>
#include <string>
#include <vector>

namespace pdxinfo::detail {

class OdxServiceParser {
public:
    OdxServiceParser(const OdxNodeReader& reader, const OdxMessageParser& messageParser);

    DiagnosticService parseService(
        const xml::Node& serviceNode,
        const std::map<std::string, const xml::Node*>& idIndex) const;
    std::vector<DiagnosticService> parseServicesUnder(
        const xml::Node& root,
        const std::map<std::string, const xml::Node*>& idIndex) const;

private:
    void addReferencedMessages(const xml::Node& serviceNode,
        const std::map<std::string, const xml::Node*>& idIndex,
        const std::set<std::string>& refNodeNames,
        std::vector<Message>& destination) const;

    const OdxNodeReader& reader_;
    const OdxMessageParser& messageParser_;
};

}
