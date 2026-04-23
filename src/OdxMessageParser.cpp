#include "pdxinfo/OdxMessageParser.hpp"

namespace pdxinfo::detail {

OdxMessageParser::OdxMessageParser(const OdxNodeReader& reader)
    : reader_(reader) {
}

Parameter OdxMessageParser::parseParameter(const xml::Node& node) const {
    Parameter parameter;
    parameter.shortName = reader_.shortNameOf(node);
    parameter.semantic = node.childTextLocal("SEMANTIC");
    if (parameter.semantic.empty()) {
        parameter.semantic = reader_.attr(node, "SEMANTIC");
    }

    if (const auto bytePosition = reader_.parseInteger(node.childTextLocal("BYTE-POSITION"))) {
        parameter.bytePosition = static_cast<int>(*bytePosition);
    }
    parameter.codedValue = reader_.parseInteger(node.childTextLocal("CODED-VALUE"));
    parameter.rawElement = xml::localName(node.name);

    if (const auto* dopRef = node.firstChildLocal("DOP-REF")) {
        parameter.dopRef = reader_.refId(*dopRef);
    }
    if (const auto* tableRef = node.firstChildLocal("TABLE-REF")) {
        parameter.tableRef = reader_.refId(*tableRef);
    }

    return parameter;
}

Message OdxMessageParser::parseMessage(const xml::Node& node) const {
    Message message;
    message.id = reader_.nodeId(node);
    message.shortName = reader_.shortNameOf(node);
    for (const auto* paramNode : reader_.collectByLocalName(node, {"PARAM"})) {
        message.parameters.push_back(parseParameter(*paramNode));
    }
    return message;
}

std::optional<std::uint64_t> OdxMessageParser::findSid(const std::vector<Message>& messages) const {
    for (const auto& message : messages) {
        for (const auto& parameter : message.parameters) {
            const auto semantic = reader_.upper(parameter.semantic);
            const auto name = reader_.upper(parameter.shortName);
            if (parameter.codedValue && (semantic.find("SERVICE-ID") != std::string::npos ||
                    semantic == "SID" || name == "SID" || name.find("SERVICE") != std::string::npos)) {
                return parameter.codedValue;
            }
        }
    }
    return std::nullopt;
}

}
