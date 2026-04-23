#include "pdxinfo/OdxParser.hpp"

#include "pdxinfo/OdxDidExtractor.hpp"
#include "pdxinfo/OdxDtcParser.hpp"
#include "pdxinfo/OdxMessageParser.hpp"
#include "pdxinfo/OdxNodeReader.hpp"
#include "pdxinfo/OdxServiceParser.hpp"

#include "pdxinfo/Xml.hpp"

#include <map>
#include <set>
#include <stdexcept>

namespace pdxinfo {
namespace {

using Node = xml::Node;

std::map<std::string, const Node*> buildIdIndex(
    const std::vector<const Node*>& nodes,
    const detail::OdxNodeReader& reader) {
    std::map<std::string, const Node*> idIndex;
    for (const auto* node : nodes) {
        const auto id = reader.nodeId(*node);
        if (!id.empty()) {
            idIndex[id] = node;
        }
    }
    return idIndex;
}

std::vector<Ecu> parseEcus(
    const Node& root,
    const std::map<std::string, const Node*>& idIndex,
    const detail::OdxNodeReader& reader,
    const detail::OdxServiceParser& serviceParser) {
    std::vector<Ecu> ecus;
    std::set<std::string> seen;

    for (const auto* ecuNode : reader.collectByLocalName(root, {"ECU-VARIANT", "BASE-VARIANT", "PROTOCOL", "FUNCTIONAL-GROUP"})) {
        const auto id = reader.nodeId(*ecuNode);
        if (!id.empty() && seen.count(id) > 0) {
            continue;
        }
        if (!id.empty()) {
            seen.insert(id);
        }

        Ecu ecu;
        ecu.id = id;
        ecu.shortName = reader.shortNameOf(*ecuNode);
        ecu.type = xml::localName(ecuNode->name);
        ecu.services = serviceParser.parseServicesUnder(*ecuNode, idIndex);
        ecus.push_back(std::move(ecu));
    }

    return ecus;
}

std::vector<Dtc> parseDtcs(
    const Node& root,
    const detail::OdxNodeReader& reader,
    const detail::OdxDtcParser& dtcParser) {
    std::vector<Dtc> dtcs;
    std::set<std::string> seen;

    for (const auto* dtcNode : reader.collectByLocalName(root, {"DTC"})) {
        const auto id = reader.nodeId(*dtcNode);
        if (!id.empty() && seen.count(id) > 0) {
            continue;
        }
        if (!id.empty()) {
            seen.insert(id);
        }
        dtcs.push_back(dtcParser.parseDtc(*dtcNode));
    }

    return dtcs;
}

}

OdxDocument OdxParser::parse(const std::string& sourceName, const std::string& xmlText) const {
    auto root = xml::parse(xmlText);
    if (!root) {
        throw std::runtime_error("Failed to parse XML");
    }

    const detail::OdxNodeReader reader;
    const detail::OdxMessageParser messageParser(reader);
    const detail::OdxServiceParser serviceParser(reader, messageParser);
    const detail::OdxDtcParser dtcParser(reader);
    const detail::OdxDidExtractor didExtractor(reader);

    const auto idIndex = buildIdIndex(reader.collectAll(*root), reader);

    OdxDocument document;
    document.sourceName = sourceName;
    document.ecus = parseEcus(*root, idIndex, reader, serviceParser);
    document.looseServices = serviceParser.parseServicesUnder(*root, idIndex);
    document.dtcs = parseDtcs(*root, reader, dtcParser);
    document.dids = didExtractor.extractFromServices(document.looseServices);

    return document;
}

}
