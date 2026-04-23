#pragma once

#include "pdxinfo/OdxDataModel.hpp"

#include <iosfwd>

namespace pdxinfo {

enum class ReportMode {
    Full,
    Summary,
    Services,
    Dtcs,
    Dids
};

void writeReport(std::ostream& out, const DiagnosticDatabase& database, ReportMode mode);

}
