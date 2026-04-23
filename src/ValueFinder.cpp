#include "pdxinfo/ValueFinder.hpp"

#include "pdxinfo/Bytes.hpp"
#include "pdxinfo/Xml.hpp"

#include <algorithm>
#include <cctype>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <sstream>

namespace pdxinfo {
namespace {

using Node = xml::Node;

struct DopInfo {
    std::string id;
    std::string shortName;
    std::string longName;
    std::string formula;
};

struct ValueParam {
    std::string shortName;
    std::string longName;
    std::string semantic;
    std::optional<int> bytePosition;
    std::optional<std::uint64_t> codedValue;
    std::string dopRef;
};

struct ValueCandidate {
    std::string sourceName;
    std::string id;
    std::string shortName;
    std::string longName;
    std::optional<int> pid;
    std::optional<int> byteSize;
    std::vector<ValueParam> params;
};

struct NodeTarget {
    const Node* node = nullptr;
    std::string sourceName;
};

struct ParsedDocument {
    std::string sourceName;
    std::unique_ptr<Node> root;
};

struct PackageIndex {
    std::vector<ParsedDocument> documents;
    std::map<std::string, NodeTarget> nodesById;
    std::map<std::string, DopInfo> dopsById;
};

struct ValueFinding {
    std::string queryText;
    std::string summary;
    std::string details;
};

struct ValueQueryFindings {
    std::string queryText;
    std::vector<ValueFinding> matches;
};

std::string lower(std::string value) {
    for (auto& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

std::string attr(const Node& node, const std::string& name) {
    const auto it = node.attributes.find(name);
    if (it != node.attributes.end()) {
        return it->second;
    }
    return {};
}

std::string nodeId(const Node& node) {
    for (const auto* candidate : {"ID", "OID", "UUID"}) {
        const auto value = attr(node, candidate);
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string refId(const Node& node) {
    for (const auto* candidate : {"ID-REF", "DOCREF", "OID-REF"}) {
        const auto value = attr(node, candidate);
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string textOf(const Node& node) {
    std::string text = node.text;
    for (const auto& child : node.children) {
        const auto childText = textOf(*child);
        if (!childText.empty()) {
            if (!text.empty()) {
                text += " ";
            }
            text += childText;
        }
    }
    return xml::trim(text);
}

std::string shortNameOf(const Node& node) {
    auto value = node.childTextLocal("SHORT-NAME");
    if (value.empty()) {
        value = nodeId(node);
    }
    return value;
}

std::string longNameOf(const Node& node) {
    return node.childTextLocal("LONG-NAME");
}

std::optional<int> parseInt(const std::string& text) {
    if (xml::trim(text).empty()) {
        return std::nullopt;
    }
    try {
        std::size_t processed = 0;
        const auto value = std::stoi(xml::trim(text), &processed, 0);
        if (processed == xml::trim(text).size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<std::uint64_t> parseUInt(const std::string& text) {
    if (xml::trim(text).empty()) {
        return std::nullopt;
    }
    try {
        std::size_t processed = 0;
        const auto value = std::stoull(xml::trim(text), &processed, 0);
        if (processed == xml::trim(text).size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<int> extractPid(const std::string& text) {
    static const std::regex pattern(R"(PID\s+\$?([0-9A-Fa-f]{2}))");
    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        return std::stoi(match[1].str(), nullptr, 16);
    }
    return std::nullopt;
}

std::string extractFormula(const std::string& text) {
    std::smatch match;
    static const std::regex fractionPattern(R"(\(([0-9]+)\s*/\s*([0-9]+)\)\s*x)", std::regex::icase);
    if (std::regex_search(text, match, fractionPattern)) {
        return "physical = raw * " + match[1].str() + " / " + match[2].str();
    }

    static const std::regex decimalPattern(R"(([0-9]+(?:\.[0-9]+)?)\s*x)", std::regex::icase);
    if (std::regex_search(text, match, decimalPattern)) {
        return "physical = raw * " + match[1].str();
    }

    static const std::regex perBitPattern(R"(([0-9]+(?:\.[0-9]+)?)\s+per bit)", std::regex::icase);
    if (std::regex_search(text, match, perBitPattern)) {
        return "physical = raw * " + match[1].str();
    }

    return {};
}

void collectByLocalName(const Node& node, const std::set<std::string>& names, std::vector<const Node*>& out) {
    if (names.count(xml::localName(node.name)) > 0) {
        out.push_back(&node);
    }
    for (const auto& child : node.children) {
        collectByLocalName(*child, names, out);
    }
}

void collectAll(const Node& node, std::vector<const Node*>& out) {
    out.push_back(&node);
    for (const auto& child : node.children) {
        collectAll(*child, out);
    }
}

DopInfo parseDop(const Node& node) {
    DopInfo dop;
    dop.id = nodeId(node);
    dop.shortName = shortNameOf(node);
    dop.longName = longNameOf(node);
    dop.formula = extractFormula(textOf(node));
    return dop;
}

ValueParam parseParam(const Node& node) {
    ValueParam param;
    param.shortName = shortNameOf(node);
    param.longName = longNameOf(node);
    param.semantic = node.childTextLocal("SEMANTIC");
    if (param.semantic.empty()) {
        param.semantic = attr(node, "SEMANTIC");
    }
    param.bytePosition = parseInt(node.childTextLocal("BYTE-POSITION"));
    param.codedValue = parseUInt(node.childTextLocal("CODED-VALUE"));
    if (const auto* dopRef = node.firstChildLocal("DOP-REF")) {
        param.dopRef = refId(*dopRef);
    }
    return param;
}

ValueCandidate parseStructure(const Node& node, const std::string& sourceName) {
    ValueCandidate candidate;
    candidate.sourceName = sourceName;
    candidate.id = nodeId(node);
    candidate.shortName = shortNameOf(node);
    candidate.longName = longNameOf(node);
    candidate.pid = extractPid(candidate.longName + " " + candidate.shortName);
    candidate.byteSize = parseInt(node.childTextLocal("BYTE-SIZE"));

    std::vector<const Node*> params;
    collectByLocalName(node, {"PARAM"}, params);
    for (const auto* paramNode : params) {
        candidate.params.push_back(parseParam(*paramNode));
    }

    return candidate;
}

std::string expandedText(const Node& node,
    const PackageIndex& index,
    int depth = 0) {
    std::string text = node.name + " " + textOf(node);
    if (depth >= 3) {
        return text;
    }

    std::vector<const Node*> refs;
    collectByLocalName(node, {"REQUEST-REF", "POS-RESPONSE-REF", "NEG-RESPONSE-REF", "DOP-REF", "STRUCTURE-REF", "TABLE-REF"}, refs);
    std::set<std::string> seen;
    for (const auto* ref : refs) {
        const auto id = refId(*ref);
        if (id.empty() || !seen.insert(id).second) {
            continue;
        }
        const auto it = index.nodesById.find(id);
        if (it != index.nodesById.end() && it->second.node) {
            text += " " + expandedText(*it->second.node, index, depth + 1);
        }
    }
    return text;
}

std::vector<NodeTarget> referencedNodes(const Node& node,
    const PackageIndex& index,
    const std::set<std::string>& refNames) {
    std::vector<const Node*> refs;
    collectByLocalName(node, refNames, refs);

    std::vector<NodeTarget> result;
    std::set<std::string> seen;
    for (const auto* ref : refs) {
        const auto id = refId(*ref);
        if (id.empty() || !seen.insert(id).second) {
            continue;
        }
        const auto it = index.nodesById.find(id);
        if (it != index.nodesById.end() && it->second.node) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::vector<ValueParam> paramsOf(const Node& node) {
    std::vector<const Node*> paramNodes;
    collectByLocalName(node, {"PARAM"}, paramNodes);

    std::vector<ValueParam> params;
    for (const auto* paramNode : paramNodes) {
        params.push_back(parseParam(*paramNode));
    }
    return params;
}

int encodedByteCount(std::uint64_t value) {
    if (value <= 0xFF) {
        return 1;
    }
    if (value <= 0xFFFF) {
        return 2;
    }
    if (value <= 0xFFFFFF) {
        return 3;
    }
    return 4;
}

std::vector<std::optional<std::uint8_t>> bytesFromCodedParams(const std::vector<ValueParam>& params) {
    std::vector<std::optional<std::uint8_t>> bytes;
    for (const auto& param : params) {
        if (!param.codedValue) {
            continue;
        }

        const auto bytePosition = param.bytePosition.value_or(static_cast<int>(bytes.size()));
        const auto byteCount = encodedByteCount(*param.codedValue);
        if (bytes.size() < static_cast<std::size_t>(bytePosition + byteCount)) {
            bytes.resize(bytePosition + byteCount);
        }

        for (int i = 0; i < byteCount; ++i) {
            const auto shift = 8 * (byteCount - i - 1);
            bytes[bytePosition + i] = static_cast<std::uint8_t>((*param.codedValue >> shift) & 0xFF);
        }
    }
    return bytes;
}

std::string formatMessageBytes(const std::vector<std::optional<std::uint8_t>>& bytes) {
    if (bytes.empty()) {
        return "not inferred automatically";
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) {
            out << ' ';
        }
        if (bytes[i]) {
            out << toHex(*bytes[i], 2).substr(2);
        } else {
            out << "B" << i;
        }
    }
    return out.str();
}

std::optional<int> inferByteCount(const DopInfo& dop);

void markVariableResponseBytes(std::vector<std::optional<std::uint8_t>>& bytes,
    const std::vector<ValueParam>& params,
    const std::map<std::string, DopInfo>& dops) {
    for (const auto& param : params) {
        if (param.codedValue || !param.bytePosition) {
            continue;
        }

        int byteCount = 1;
        if (!param.dopRef.empty()) {
            const auto it = dops.find(param.dopRef);
            if (it != dops.end()) {
                byteCount = inferByteCount(it->second).value_or(byteCount);
            }
        }

        if (bytes.size() < static_cast<std::size_t>(*param.bytePosition + byteCount)) {
            bytes.resize(*param.bytePosition + byteCount);
        }
    }
}

std::optional<int> inferByteCount(const DopInfo& dop) {
    static const std::regex bytesPattern(R"(([0-9]+)\s*Bytes?)", std::regex::icase);
    std::smatch match;
    if (std::regex_search(dop.longName, match, bytesPattern)) {
        return std::stoi(match[1].str());
    }
    return std::nullopt;
}

void writeRawExpression(std::ostream& out, int bytePosition, int byteCount) {
    if (byteCount <= 0) {
        return;
    }

    out << "    Raw: raw = ";
    if (byteCount == 1) {
        out << "B" << bytePosition << '\n';
        return;
    }

    for (int i = 0; i < byteCount; ++i) {
        if (i > 0) {
            out << " | ";
        }
        const int shift = 8 * (byteCount - i - 1);
        out << "(B" << (bytePosition + i);
        if (shift > 0) {
            out << " << " << shift;
        }
        out << ")";
    }
    out << '\n';
}

void writeFormula(std::ostream& out, const ValueParam& param, const std::map<std::string, DopInfo>& dops) {
    if (param.dopRef.empty()) {
        return;
    }

    out << "    DOP: " << param.dopRef;
    const auto it = dops.find(param.dopRef);
    if (it != dops.end()) {
        if (!it->second.longName.empty()) {
            out << " - " << it->second.longName;
        }
        out << '\n';

        const auto byteCount = inferByteCount(it->second);
        if (param.bytePosition && byteCount) {
            writeRawExpression(out, *param.bytePosition, *byteCount);
        }

        if (!it->second.formula.empty()) {
            out << "    Formula: " << it->second.formula << '\n';
        }
        return;
    }
    out << '\n';
}

void writeCandidate(std::ostream& out, const ValueCandidate& candidate, const std::map<std::string, DopInfo>& dops) {
    out << "Source: " << candidate.sourceName << '\n';
    out << "Object: " << (candidate.longName.empty() ? candidate.shortName : candidate.longName) << '\n';
    if (!candidate.id.empty()) {
        out << "ID: " << candidate.id << '\n';
    }

    if (candidate.pid) {
        out << "Request: 01 " << toHex(*candidate.pid, 2).substr(2) << '\n';
        out << "Positive response: 41 " << toHex(*candidate.pid, 2).substr(2);
        if (candidate.byteSize) {
            for (int i = 0; i < *candidate.byteSize; ++i) {
                out << " B" << i;
            }
        } else {
            out << " ...";
        }
        out << '\n';
    } else {
        out << "Request: not inferred automatically from this object\n";
    }

    for (const auto& param : candidate.params) {
        out << "  Param: " << (param.longName.empty() ? param.shortName : param.longName);
        if (param.bytePosition) {
            out << " byte=" << *param.bytePosition;
        }
        out << '\n';
        writeFormula(out, param, dops);
    }
}

void writeServiceMatch(std::ostream& out,
    const std::string& sourceName,
    const Node& service,
    const std::vector<NodeTarget>& requests,
    const std::vector<NodeTarget>& positiveResponses,
    const std::map<std::string, DopInfo>& dops) {
    out << "Source: " << sourceName << '\n';
    out << "Service: " << shortNameOf(service);
    const auto semantic = service.childTextLocal("SEMANTIC");
    if (!semantic.empty()) {
        out << " [" << semantic << "]";
    }
    out << '\n';
    if (!nodeId(service).empty()) {
        out << "ID: " << nodeId(service) << '\n';
    }

    for (const auto& request : requests) {
        const auto requestParams = paramsOf(*request.node);
        out << "Request object: " << shortNameOf(*request.node);
        if (request.sourceName != sourceName) {
            out << " (from " << request.sourceName << ")";
        }
        out << '\n';
        out << "Request: " << formatMessageBytes(bytesFromCodedParams(requestParams)) << '\n';
    }

    for (const auto& response : positiveResponses) {
        const auto responseParams = paramsOf(*response.node);
        auto responseBytes = bytesFromCodedParams(responseParams);
        markVariableResponseBytes(responseBytes, responseParams, dops);

        out << "Positive response object: " << shortNameOf(*response.node);
        if (response.sourceName != sourceName) {
            out << " (from " << response.sourceName << ")";
        }
        out << '\n';
        out << "Positive response: " << formatMessageBytes(responseBytes) << '\n';
        for (const auto& param : responseParams) {
            if (param.codedValue) {
                continue;
            }
            out << "  Param: " << (param.longName.empty() ? param.shortName : param.longName);
            if (param.bytePosition) {
                out << " byte=" << *param.bytePosition;
            }
            out << '\n';
            writeFormula(out, param, dops);
        }
    }
}

PackageIndex buildPackageIndex(std::ostream& out, const std::vector<RawDocument>& documents) {
    PackageIndex index;

    for (const auto& document : documents) {
        ParsedDocument parsed;
        parsed.sourceName = document.sourceName;
        try {
            parsed.root = xml::parse(document.content);
        } catch (const std::exception& error) {
            out << "Skipped " << document.sourceName << ": " << error.what() << "\n\n";
            continue;
        }
        index.documents.push_back(std::move(parsed));
    }

    for (const auto& document : index.documents) {
        std::vector<const Node*> allNodes;
        collectAll(*document.root, allNodes);
        for (const auto* node : allNodes) {
            const auto id = nodeId(*node);
            if (!id.empty() && index.nodesById.count(id) == 0) {
                index.nodesById[id] = NodeTarget{node, document.sourceName};
            }

            if (xml::localName(node->name) == "DATA-OBJECT-PROP") {
                auto dop = parseDop(*node);
                if (!dop.id.empty() && index.dopsById.count(dop.id) == 0) {
                    index.dopsById[dop.id] = std::move(dop);
                }
            }
        }
    }

    return index;
}

std::string displayName(const std::string& longName, const std::string& shortName, const std::string& fallback) {
    if (!longName.empty()) {
        return longName;
    }
    if (!shortName.empty()) {
        return shortName;
    }
    return fallback;
}

ValueFinding makeServiceFinding(const std::string& queryText,
    std::size_t matchNumber,
    const std::string& sourceName,
    const Node& service,
    const std::vector<NodeTarget>& requests,
    const std::vector<NodeTarget>& positiveResponses,
    const std::map<std::string, DopInfo>& dops) {
    std::ostringstream details;
    details << "Match " << matchNumber << " (diagnostic service)\n";
    writeServiceMatch(details, sourceName, service, requests, positiveResponses, dops);
    details << '\n';

    ValueFinding finding;
    finding.queryText = queryText;
    finding.summary = "diagnostic service: " + displayName({}, shortNameOf(service), "<unnamed service>") +
                      " [" + sourceName + "]";
    finding.details = details.str();
    return finding;
}

ValueFinding makeStructureFinding(const std::string& queryText,
    std::size_t matchNumber,
    const ValueCandidate& candidate,
    const std::map<std::string, DopInfo>& dops) {
    std::ostringstream details;
    details << "Match " << matchNumber << " (data structure)\n";
    writeCandidate(details, candidate, dops);
    details << '\n';

    ValueFinding finding;
    finding.queryText = queryText;
    finding.summary = "data structure: " +
                      displayName(candidate.longName, candidate.shortName, "<unnamed structure>") +
                      " [" + candidate.sourceName + "]";
    finding.details = details.str();
    return finding;
}

std::vector<ValueQueryFindings> collectValueFindings(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries) {
    std::vector<ValueQueryFindings> result;
    const auto index = buildPackageIndex(out, documents);

    for (const auto& query : queries) {
        ValueQueryFindings queryResult;
        queryResult.queryText = query.text;

        for (const auto& document : index.documents) {
            std::vector<const Node*> services;
            collectByLocalName(*document.root, {"DIAG-SERVICE"}, services);
            std::set<std::string> seenServices;
            for (const auto* service : services) {
                const auto searchable = expandedText(*service, index);
                if (!containsIgnoreCase(searchable, query.text)) {
                    continue;
                }

                const auto key = document.sourceName + "#service#" + nodeId(*service);
                if (!seenServices.insert(key).second) {
                    continue;
                }

                const auto requests = referencedNodes(*service, index, {"REQUEST-REF"});
                const auto positiveResponses = referencedNodes(*service, index, {"POS-RESPONSE-REF"});
                const auto matchNumber = queryResult.matches.size() + 1;
                queryResult.matches.push_back(makeServiceFinding(query.text,
                    matchNumber,
                    document.sourceName,
                    *service,
                    requests,
                    positiveResponses,
                    index.dopsById));
            }

            std::vector<const Node*> structures;
            collectByLocalName(*document.root, {"STRUCTURE"}, structures);
            std::set<std::string> seenStructures;
            for (const auto* structure : structures) {
                const auto searchable = shortNameOf(*structure) + " " + longNameOf(*structure) + " " + textOf(*structure);
                if (!containsIgnoreCase(searchable, query.text)) {
                    continue;
                }

                auto candidate = parseStructure(*structure, document.sourceName);
                const auto key = document.sourceName + "#" + candidate.id;
                if (!seenStructures.insert(key).second) {
                    continue;
                }

                const auto matchNumber = queryResult.matches.size() + 1;
                queryResult.matches.push_back(makeStructureFinding(query.text, matchNumber, candidate, index.dopsById));
            }
        }

        result.push_back(std::move(queryResult));
    }

    return result;
}

std::size_t totalMatchCount(const std::vector<ValueQueryFindings>& queryFindings) {
    std::size_t total = 0;
    for (const auto& query : queryFindings) {
        total += query.matches.size();
    }
    return total;
}

std::vector<const ValueFinding*> flattenFindings(const std::vector<ValueQueryFindings>& queryFindings) {
    std::vector<const ValueFinding*> matches;
    for (const auto& query : queryFindings) {
        for (const auto& match : query.matches) {
            matches.push_back(&match);
        }
    }
    return matches;
}

}

void writeValueFindings(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries) {
    const auto queryFindings = collectValueFindings(out, documents, queries);

    for (const auto& query : queryFindings) {
        out << "Value query: " << query.queryText << "\n\n";
        for (const auto& match : query.matches) {
            out << match.details;
        }

        if (query.matches.empty()) {
            out << "No structured value matches found. Try --search \"" << query.queryText
                << "\" for raw XML matches.\n";
        }
        out << "Structured matches: " << query.matches.size() << "\n\n";
    }

    out << "Total structured matches: " << totalMatchCount(queryFindings) << '\n';
}

void writeInteractiveValueFindings(std::istream& in,
    std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries) {
    const auto queryFindings = collectValueFindings(out, documents, queries);
    const auto matches = flattenFindings(queryFindings);

    if (matches.empty()) {
        for (const auto& query : queryFindings) {
            out << "Value query: " << query.queryText << '\n';
            out << "No structured value matches found. Try --search \"" << query.queryText
                << "\" for raw XML matches.\n\n";
        }
        out << "Total structured matches: 0\n";
        return;
    }

    out << "Structured value matches\n";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        out << (i + 1) << ". " << matches[i]->summary << " (query: " << matches[i]->queryText << ")\n";
    }
    out << '\n';

    while (true) {
        out << "Choose a match number to display, or 0 to exit: ";
        out.flush();

        std::string choice;
        if (!std::getline(in, choice)) {
            out << '\n';
            return;
        }

        const auto selected = parseInt(choice);
        if (!selected || *selected < 0 || static_cast<std::size_t>(*selected) > matches.size()) {
            out << "Please enter a number from 0 to " << matches.size() << ".\n";
            continue;
        }

        if (*selected == 0) {
            return;
        }

        out << '\n' << matches[static_cast<std::size_t>(*selected - 1)]->details;
    }
}

}
