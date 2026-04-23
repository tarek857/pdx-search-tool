#include "pdxinfo/OdxDidExtractor.hpp"

#include <set>

namespace pdxinfo::detail {

OdxDidExtractor::OdxDidExtractor(const OdxNodeReader& reader)
    : reader_(reader) {
}

std::vector<Did> OdxDidExtractor::extractFromServices(const std::vector<DiagnosticService>& services) const {
    std::vector<Did> dids;
    std::set<std::uint64_t> seen;
    for (const auto& service : services) {
        addDidCandidates(service, dids, seen);
    }
    return dids;
}

void OdxDidExtractor::addDidCandidates(
    const DiagnosticService& service,
    std::vector<Did>& dids,
    std::set<std::uint64_t>& seen) const {
    const auto semantic = reader_.upper(service.semantic + " " + service.shortName);
    if (semantic.find("IDENTIFIER") == std::string::npos && service.requestSid.value_or(0) != 0x22 &&
        service.requestSid.value_or(0) != 0x2E) {
        return;
    }

    for (const auto& message : service.requests) {
        for (const auto& parameter : message.parameters) {
            const auto parameterSemantic = reader_.upper(parameter.semantic + " " + parameter.shortName);
            if (!parameter.codedValue) {
                continue;
            }
            if (parameterSemantic.find("ID") != std::string::npos ||
                parameterSemantic.find("IDENTIFIER") != std::string::npos ||
                parameter.shortName.find("DID") != std::string::npos) {
                const auto value = *parameter.codedValue;
                if (value > 0xFF && seen.insert(value).second) {
                    dids.push_back(Did{parameter.shortName, value, service.shortName});
                }
            }
        }
    }
}

}
