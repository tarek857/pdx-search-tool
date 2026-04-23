#pragma once

#include "OdxNodeReader.hpp"

#include "pdxinfo/OdxDataModel.hpp"

#include <cstdint>
#include <set>
#include <vector>

namespace pdxinfo::detail {

class OdxDidExtractor {
public:
    explicit OdxDidExtractor(const OdxNodeReader& reader);

    std::vector<Did> extractFromServices(const std::vector<DiagnosticService>& services) const;

private:
    void addDidCandidates(
        const DiagnosticService& service,
        std::vector<Did>& dids,
        std::set<std::uint64_t>& seen) const;

    const OdxNodeReader& reader_;
};

}
