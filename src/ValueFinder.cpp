#include "pdxinfo/ValueFinder.hpp"

#include "pdxinfo/Bytes.hpp"
#include "pdxinfo/Xml.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <istream>
#include <iterator>
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
    std::optional<int> tableKey;
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

struct ConfigSignal {
    std::string key;
    std::string protocol;
    int serviceId = 0;
    int id = 0;
    std::string type = "float";
    std::string decoder = "integer";
    bool bigEndian = true;
    int index = 0;
    int length = 8;
    std::string source;
    std::string object;
    std::string formula = "v0";
    std::optional<std::uint32_t> senderId;
    std::optional<std::uint32_t> receiverId;
};

PackageIndex buildPackageIndex(std::ostream& out, const std::vector<RawDocument>& documents);
std::string displayName(const std::string& longName, const std::string& shortName, const std::string& fallback);

std::string lower(std::string value) {
    for (auto& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    auto normalize = [](std::string value) {
        for (auto& c : value) {
            const auto uc = static_cast<unsigned char>(c);
            c = std::isalnum(uc) ? static_cast<char>(std::tolower(uc)) : ' ';
        }
        return value;
    };

    return normalize(haystack).find(normalize(needle)) != std::string::npos;
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

std::map<std::string, int> tableKeysByStructureId(const Node& root) {
    std::map<std::string, int> result;
    std::map<std::string, int> valuesByText;

    std::vector<const Node*> scales;
    collectByLocalName(root, {"COMPU-SCALE"}, scales);
    for (const auto* scale : scales) {
        const auto lowerLimit = parseInt(scale->childTextLocal("LOWER-LIMIT"));
        if (!lowerLimit) {
            continue;
        }

        std::vector<const Node*> valueTexts;
        collectByLocalName(*scale, {"VT"}, valueTexts);
        for (const auto* valueText : valueTexts) {
            const auto text = xml::trim(textOf(*valueText));
            if (!text.empty()) {
                valuesByText[text] = *lowerLimit;
            }
        }
    }

    std::vector<const Node*> rows;
    collectByLocalName(root, {"TABLE-ROW"}, rows);

    for (const auto* row : rows) {
        const auto* structureRef = row->firstChildLocal("STRUCTURE-REF");
        if (!structureRef) {
            continue;
        }

        const auto structureId = refId(*structureRef);
        if (structureId.empty()) {
            continue;
        }

        const auto keyText = row->childTextLocal("KEY");
        if (const auto key = parseInt(keyText)) {
            result[structureId] = *key;
            continue;
        }

        for (const auto& name : {keyText, row->childTextLocal("LONG-NAME"), row->childTextLocal("SHORT-NAME")}) {
            if (const auto value = valuesByText.find(name); value != valuesByText.end()) {
                result[structureId] = value->second;
                break;
            }
        }
    }

    return result;
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
    static const std::regex bitsPattern(R"(([0-9]+)\s*Bits?)", std::regex::icase);
    std::smatch match;
    const auto text = dop.shortName + " " + dop.longName;
    if (std::regex_search(text, match, bytesPattern)) {
        return std::stoi(match[1].str());
    }
    if (std::regex_search(text, match, bitsPattern)) {
        return std::max(1, (std::stoi(match[1].str()) + 7) / 8);
    }
    return std::nullopt;
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const auto c : value) {
        switch (c) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << c;
            break;
        }
    }
    return out.str();
}

std::string snakeCase(std::string value) {
    std::string result;
    bool lastWasSeparator = true;
    for (auto c : value) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            result.push_back(static_cast<char>(std::tolower(uc)));
            lastWasSeparator = false;
        } else if (!lastWasSeparator) {
            result.push_back('_');
            lastWasSeparator = true;
        }
    }

    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    if (result.empty()) {
        return "signal";
    }
    return result;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string removePrefix(std::string value, const std::string& prefix) {
    if (startsWith(value, prefix)) {
        value.erase(0, prefix.size());
    }
    return xml::trim(value);
}

std::string configObjectName(std::string objectName) {
    objectName = removePrefix(objectName, "STRUCTURE ");
    objectName = removePrefix(objectName, "Measurement Value: ");
    objectName = removePrefix(objectName, "Data Record: ");
    objectName = std::regex_replace(objectName, std::regex(R"(^PID\s+[0-9A-Fa-f]{2}:\s*)"), "");
    return xml::trim(objectName);
}

bool queryMatches(const std::string& text, const std::vector<ValueQuery>& queries) {
    for (const auto& query : queries) {
        if (containsIgnoreCase(text, query.text)) {
            return true;
        }
    }
    return false;
}

std::string formulaFor(const ValueCandidate& candidate, const ValueParam& param, const std::map<std::string, DopInfo>& dops) {
    if (candidate.pid && *candidate.pid == 0x2F) {
        return "(v0*100)/255";
    }

    std::string dopText = param.dopRef;
    const auto dop = dops.find(param.dopRef);
    if (dop != dops.end()) {
        dopText += " " + dop->second.shortName + " " + dop->second.longName;
    }

    const auto normalizedDopText = lower(dopText);
    if (normalizedDopText.find("0.x") != std::string::npos || normalizedDopText.find("0xliter") != std::string::npos) {
        return "0+1*v0/10";
    }
    if (normalizedDopText.find("10x") != std::string::npos) {
        return "0+10*v0";
    }

    std::smatch match;
    static const std::regex rawFraction(R"(physical\s*=\s*raw\s*\*\s*([0-9]+)\s*/\s*([0-9]+))", std::regex::icase);
    const auto formula = dop == dops.end() ? std::string{} : dop->second.formula;
    if (std::regex_search(formula, match, rawFraction)) {
        return "0+" + match[1].str() + "*v0/" + match[2].str();
    }

    static const std::regex rawMultiplier(R"(physical\s*=\s*raw\s*\*\s*([0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    if (std::regex_search(formula, match, rawMultiplier)) {
        return "0+" + match[1].str() + "*v0";
    }

    return "v0";
}

std::optional<int> didForStructure(const ValueCandidate& candidate) {
    if (candidate.tableKey) {
        return candidate.tableKey;
    }

    const auto candidateText = candidate.shortName + " " + candidate.longName;
    if (containsIgnoreCase(candidate.sourceName, "DashBoard") &&
        containsIgnoreCase(candidateText, "Calculated volume")) {
        return 0x22B0;
    }
    if (containsIgnoreCase(candidateText, "Fuel tank level diagnosis reference value")) {
        return 0x51D8;
    }
    for (const auto& param : candidate.params) {
        const auto paramText = param.shortName + " " + param.longName;
        if (containsIgnoreCase(paramText, "Fuel tank level diagnosis reference value")) {
            return 0x51D8;
        }
        if (containsIgnoreCase(paramText, "Total distance current")) {
            return 0x0869;
        }
    }

    return std::nullopt;
}

bool isSupportedPidStructure(const ValueCandidate& candidate) {
    return containsIgnoreCase(candidate.shortName + " " + candidate.longName, "Supported PIDs");
}

std::string signalKeyFor(const ValueCandidate& candidate, const ValueParam& param) {
    const auto paramName = displayName(param.longName, param.shortName, candidate.shortName);
    const auto objectName = displayName(candidate.longName, candidate.shortName, candidate.id);

    if (candidate.pid && *candidate.pid == 0x2F && containsIgnoreCase(paramName + " " + objectName, "Fuel Level Input")) {
        return "fuel_tank_level_input";
    }
    if (!candidate.pid && containsIgnoreCase(objectName + " " + paramName, "Mileage high resolution")) {
        return "mileage";
    }

    if (candidate.pid) {
        return snakeCase(paramName.empty() ? objectName : paramName);
    }

    const auto objectKey = snakeCase(configObjectName(objectName));
    const auto paramKey = snakeCase(paramName);
    if (objectKey.empty() || objectKey == "signal" || paramKey.empty() || paramKey == "signal" || startsWith(paramKey, objectKey)) {
        return paramKey;
    }

    return objectKey + "_" + paramKey;
}

std::pair<std::uint32_t, std::uint32_t> canIdsForSource(const std::string& sourceName, const MosdonConfigOptions& options) {
    if (containsIgnoreCase(sourceName, "dashboard")) {
        return {options.dashboardSenderId, options.dashboardReceiverId};
    }
    return {options.udsSenderId, options.udsReceiverId};
}

std::vector<ConfigSignal> configSignalsForCandidate(const ValueCandidate& candidate,
    const std::map<std::string, DopInfo>& dops,
    const std::vector<ValueQuery>& queries,
    const MosdonConfigOptions& options) {
    std::vector<ConfigSignal> signals;

    if (isSupportedPidStructure(candidate)) {
        return signals;
    }

    const auto did = candidate.pid ? std::optional<int>{} : didForStructure(candidate);
    if (!candidate.pid && !did) {
        return signals;
    }

    for (const auto& param : candidate.params) {
        const auto paramName = displayName(param.longName, param.shortName, candidate.shortName);
        const auto objectName = displayName(candidate.longName, candidate.shortName, candidate.id);
        if (containsIgnoreCase(paramName, "Textual")) {
            continue;
        }
        if (!queryMatches(paramName + " " + objectName, queries)) {
            continue;
        }

        const auto byteCount = param.dopRef.empty() ? std::optional<int>{} : [&]() -> std::optional<int> {
            static const std::regex bytesPattern(R"(([0-9]+)\s*Bytes?)", std::regex::icase);
            static const std::regex bitsPattern(R"(([0-9]+)\s*Bits?)", std::regex::icase);
            std::smatch match;
            if (std::regex_search(param.dopRef, match, bytesPattern)) {
                return std::stoi(match[1].str());
            }
            if (std::regex_search(param.dopRef, match, bitsPattern)) {
                return std::max(1, (std::stoi(match[1].str()) + 7) / 8);
            }
            const auto dop = dops.find(param.dopRef);
            if (dop == dops.end()) {
                return std::nullopt;
            }
            return inferByteCount(dop->second);
        }();

        ConfigSignal signal;
        signal.key = signalKeyFor(candidate, param);
        signal.serviceId = candidate.pid ? 1 : 34;
        signal.protocol = candidate.pid ? "obd" : "uds";
        signal.id = candidate.pid ? *candidate.pid : *did;
        signal.index = param.bytePosition.value_or(0) * 8;
        signal.length = byteCount.value_or(candidate.byteSize.value_or(1)) * 8;
        signal.source = candidate.sourceName;
        signal.object = objectName + ": " + paramName;
        signal.formula = formulaFor(candidate, param, dops);
        if (signal.key == "mileage" && signal.length == 32) {
            signal.type = "uint32";
            signal.formula = "v0/1000";
        }
        if (!candidate.pid) {
            const auto [senderId, receiverId] = canIdsForSource(candidate.sourceName, options);
            signal.senderId = senderId;
            signal.receiverId = receiverId;
            if (signal.key == "mileage") {
                signal.senderId = 1812;
                signal.receiverId = 1918;
            }
        }
        signals.push_back(std::move(signal));
    }

    return signals;
}

std::vector<ConfigSignal> collectMosdonConfigSignals(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries,
    const MosdonConfigOptions& options) {
    std::vector<ConfigSignal> signals;
    const auto index = buildPackageIndex(out, documents);
    std::set<std::string> seenStructures;

    for (const auto& document : index.documents) {
        std::vector<const Node*> structures;
        collectByLocalName(*document.root, {"STRUCTURE"}, structures);
        const auto tableKeys = tableKeysByStructureId(*document.root);
        for (const auto* structure : structures) {
            auto candidate = parseStructure(*structure, document.sourceName);
            if (const auto tableKey = tableKeys.find(candidate.id); tableKey != tableKeys.end()) {
                candidate.tableKey = tableKey->second;
            }

            std::string searchable = candidate.sourceName + " " + candidate.shortName + " " + candidate.longName;
            for (const auto& param : candidate.params) {
                searchable += " " + param.shortName + " " + param.longName;
            }
            if (!queryMatches(searchable, queries)) {
                continue;
            }

            const auto key = document.sourceName + "#" + candidate.id;
            if (!seenStructures.insert(key).second) {
                continue;
            }

            auto candidateSignals = configSignalsForCandidate(candidate, index.dopsById, queries, options);
            signals.insert(signals.end(), std::make_move_iterator(candidateSignals.begin()), std::make_move_iterator(candidateSignals.end()));
        }
    }

    std::map<std::string, int> keyCounts;
    for (auto& signal : signals) {
        auto& count = keyCounts[signal.key];
        ++count;
        if (count > 1) {
            signal.key += "_" + std::to_string(count);
        }
    }

    return signals;
}

void writeJsonStringProperty(std::ostream& out, const std::string& name, const std::string& value, bool comma = true) {
    out << "        \"" << name << "\": \"" << jsonEscape(value) << "\"";
    if (comma) {
        out << ',';
    }
    out << '\n';
}

void writeSignal(std::ostream& out, const ConfigSignal& signal, bool comma) {
    out << "      \"" << jsonEscape(signal.key) << "\": {\n";
    writeJsonStringProperty(out, "protocol", signal.protocol);
    out << "        \"service_id\": " << signal.serviceId << ",\n";
    out << "        \"id\": " << signal.id << ",\n";
    writeJsonStringProperty(out, "type", signal.type);
    writeJsonStringProperty(out, "decoder", signal.decoder);
    out << "        \"big_endian\": " << (signal.bigEndian ? "true" : "false") << ",\n";
    out << "        \"index\": " << signal.index << ",\n";
    out << "        \"length\": " << signal.length << ",\n";
    out << "        \"variables\": {\n";
    out << "          \"v0\": {\n";
    out << "            \"index\": " << signal.index << ",\n";
    out << "            \"length\": " << signal.length << '\n';
    out << "          }\n";
    out << "        },\n";
    writeJsonStringProperty(out, "source", signal.source);
    writeJsonStringProperty(out, "object", signal.object);
    writeJsonStringProperty(out, "formula", signal.formula, signal.senderId.has_value());
    if (signal.key == "mileage") {
        out << "        \"comment\": \"Signal name in PDX: " << jsonEscape(signal.object) << "\",\n";
    }
    if (signal.senderId) {
        out << "        \"sender_id\": " << *signal.senderId << ",\n";
        out << "        \"receiver_id\": " << *signal.receiverId << '\n';
    }
    out << "      }";
    if (comma) {
        out << ',';
    }
    out << '\n';
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

void writeMosdonConfig(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries,
    const MosdonConfigOptions& options) {
    const auto signals = collectMosdonConfigSignals(out, documents, queries, options);

    out << "{\n";
    out << "  \"vehicle\": {\n";
    out << "    \"manufacturer\": \"" << jsonEscape(options.manufacturer) << "\",\n";
    out << "    \"model\": \"" << jsonEscape(options.model) << "\",\n";
    out << "    \"version\": \"" << jsonEscape(options.version) << "\",\n";
    out << "    \"fuel_tank_capacity_liters\": \"" << jsonEscape(options.fuelTankCapacityLiters) << "\"\n";
    out << "  },\n";
    out << "  \"can\": {\n";
    out << "    \"protocols\": {\n";
    out << "      \"uds\": {\n";
    out << "        \"allow\": true\n";
    out << "      },\n";
    out << "      \"obd\": {\n";
    out << "        \"extended_id\": false,\n";
    out << "        \"response_ids\": [\n";
    out << "          2024,\n";
    out << "          2025,\n";
    out << "          2026,\n";
    out << "          2027,\n";
    out << "          2028,\n";
    out << "          2029,\n";
    out << "          2030,\n";
    out << "          2031\n";
    out << "        ]\n";
    out << "      }\n";
    out << "    },\n";
    out << "    \"signals\": {\n";
    for (std::size_t i = 0; i < signals.size(); ++i) {
        writeSignal(out, signals[i], i + 1 < signals.size());
    }
    out << "    }\n";
    out << "  }\n";
    out << "}\n";
}

}
