#pragma once

#include "pdxinfo/PackageLoader.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace pdxinfo {

struct ValueQuery {
    std::string text;
};

struct MosdonConfigOptions {
    std::string manufacturer = "Standard";
    std::string model = "11 Bit Obdii + UDS";
    std::string version = "3.1.0";
    std::string fuelTankCapacityLiters = "100";
    std::uint32_t udsSenderId = 2016;
    std::uint32_t udsReceiverId = 2024;
    std::uint32_t dashboardSenderId = 1813;
    std::uint32_t dashboardReceiverId = 1919;
};

void writeValueFindings(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries);

void writeInteractiveValueFindings(std::istream& in,
    std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries);

void writeMosdonConfig(std::ostream& out,
    const std::vector<RawDocument>& documents,
    const std::vector<ValueQuery>& queries,
    const MosdonConfigOptions& options);

}
