#pragma once

#include "pdxinfo/ValueFinder.hpp"
#include "pdxinfo/PackageLoader.hpp"
#include "pdxinfo/Report.hpp"


#include <string>

struct ProgramOptions
{
    pdxinfo::ReportMode mode = pdxinfo::ReportMode::Full;
    std::string pdxFile;
    std::string output;
    std::string searchQuery;
    std::vector<pdxinfo::ValueQuery> valueQueries;
    int contextLines = 2;
    bool interactive = false;
};
