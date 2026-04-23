#include "pdxinfo/OdxServiceParser.hpp"

#include <set>

namespace pdxinfo::detail {

OdxServiceParser::OdxServiceParser(const OdxNodeReader& reader, const OdxMessageParser& messageParser)
    : reader_(reader),
      messageParser_(messageParser) {
}

DiagnosticService OdxServiceParser::parseService(
    const xml::Node& serviceNode,
    const std::map<std::string, const xml::Node*>& idIndex) const {
    DiagnosticService service;
    service.id = reader_.nodeId(serviceNode);
    service.shortName = reader_.shortNameOf(serviceNode);
    service.semantic = serviceNode.childTextLocal("SEMANTIC");
    if (service.semantic.empty()) {
        service.semantic = reader_.attr(serviceNode, "SEMANTIC");
    }

    addReferencedMessages(serviceNode, idIndex, {"REQUEST-REF"}, service.requests);
    addReferencedMessages(serviceNode, idIndex, {"POS-RESPONSE-REF"}, service.positiveResponses);
    addReferencedMessages(serviceNode, idIndex, {"NEG-RESPONSE-REF"}, service.negativeResponses);

    if (service.requests.empty()) {
        for (const auto* child : serviceNode.childrenLocal("REQUEST")) {
            service.requests.push_back(messageParser_.parseMessage(*child));
        }
    }
    for (const auto* child : serviceNode.childrenLocal("POS-RESPONSE")) {
        service.positiveResponses.push_back(messageParser_.parseMessage(*child));
    }
    for (const auto* child : serviceNode.childrenLocal("NEG-RESPONSE")) {
        service.negativeResponses.push_back(messageParser_.parseMessage(*child));
    }

    service.requestSid = messageParser_.findSid(service.requests);
    service.positiveResponseSid = messageParser_.findSid(service.positiveResponses);
    return service;
}

std::vector<DiagnosticService> OdxServiceParser::parseServicesUnder(
    const xml::Node& root,
    const std::map<std::string, const xml::Node*>& idIndex) const {
    std::vector<DiagnosticService> services;
    std::set<std::string> seen;

    for (const auto* serviceNode : reader_.collectByLocalName(root, {"DIAG-SERVICE"})) {
        const auto id = reader_.nodeId(*serviceNode);
        if (!id.empty() && seen.count(id) > 0) {
            continue;
        }
        if (!id.empty()) {
            seen.insert(id);
        }
        services.push_back(parseService(*serviceNode, idIndex));
    }
    return services;
}

void OdxServiceParser::addReferencedMessages(
    const xml::Node& serviceNode,
    const std::map<std::string, const xml::Node*>& idIndex,
    const std::set<std::string>& refNodeNames,
    std::vector<Message>& destination) const {
    std::set<std::string> seen;
    for (const auto* refNode : reader_.collectByLocalName(serviceNode, refNodeNames)) {
        const auto id = reader_.refId(*refNode);
        if (id.empty() || seen.count(id) > 0) {
            continue;
        }
        seen.insert(id);

        const auto it = idIndex.find(id);
        if (it != idIndex.end()) {
            destination.push_back(messageParser_.parseMessage(*it->second));
        }
    }
}

}
