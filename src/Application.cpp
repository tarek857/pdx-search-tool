#include "Application.h"

#include "CommandLineParser.h"
#include "ProgramOptions.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr std::string_view usage =
    "Usage: pdx-search-tool [--summary|--services|--dtcs|--dids] [--search <text>] [--find-value <text>] [--interactive] [--context <lines>] [--output <report.txt>] --pdx-file <file.pdx|folder|file.odx>";

struct ReportModeFlags
{
    bool summary = false;
    bool services = false;
    bool dtcs = false;
    bool dids = false;
};

struct ParseResult
{
    ProgramOptions options;
    bool shouldRun = false;
    int exitCode = EXIT_SUCCESS;
};

std::vector<std::string_view> toViews(const std::vector<std::string> &arguments)
{
    std::vector<std::string_view> views;
    views.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
        views.emplace_back(argument);
    }
    return views;
}

pdxinfo::ReportMode selectedReportMode(const ReportModeFlags &flags)
{
    auto mode = pdxinfo::ReportMode::Full;
    if (flags.summary)
    {
        mode = pdxinfo::ReportMode::Summary;
    }
    if (flags.services)
    {
        mode = pdxinfo::ReportMode::Services;
    }
    if (flags.dtcs)
    {
        mode = pdxinfo::ReportMode::Dtcs;
    }
    if (flags.dids)
    {
        mode = pdxinfo::ReportMode::Dids;
    }
    return mode;
}

std::vector<pdxinfo::ValueQuery> valueQueriesFrom(const std::vector<std::string> &queryTexts)
{
    std::vector<pdxinfo::ValueQuery> queries;
    queries.reserve(queryTexts.size());
    for (const auto &query : queryTexts)
    {
        queries.push_back(pdxinfo::ValueQuery{query});
    }
    return queries;
}

void addOptions(CommandLineParser &parser,
                ProgramOptions &options,
                std::vector<std::string> &valueQueryTexts,
                bool &help,
                ReportModeFlags &modeFlags)
{
    using Option = CommandLineParser::Option;

    parser.addOption(Option{{"--help", "-h"}, &help, "Show this help message"});
    parser.addOption(Option{{"--pdx-file"}, &options.pdxFile, "Input .pdx, extracted folder, .odx, or .xml file"});
    parser.addOption(Option{{"--output", "-o"}, &options.output, "Write report to a file"});
    parser.addOption(Option{{"--search"}, &options.searchQuery, "Search raw ODX/XML text"});
    parser.addOption(Option{{"--find-value", "--value"}, &valueQueryTexts, "Find value request/response/formula; can be repeated"});
    parser.addOption(Option{{"--interactive"}, &options.interactive, "Choose one structured value match interactively"});
    parser.addOption(Option{{"--context"}, Option::integerVariant{&options.contextLines}, "Raw search context lines"});
    parser.addOption(Option{{"--summary"}, &modeFlags.summary, "Print package summary"});
    parser.addOption(Option{{"--services"}, &modeFlags.services, "Print diagnostic services"});
    parser.addOption(Option{{"--dtcs"}, &modeFlags.dtcs, "Print DTC definitions"});
    parser.addOption(Option{{"--dids"}, &modeFlags.dids, "Print DID candidates"});
}

ParseResult parseCommandLine(int argc, char **argv, std::ostream &out, std::ostream &err)
{
    ProgramOptions options;
    std::vector<std::string> valueQueryTexts;
    bool help = false;
    ReportModeFlags modeFlags;

    CommandLineParser parser(usage);
    addOptions(parser, options, valueQueryTexts, help, modeFlags);

    if (argc < 2)
    {
        parser.printHelp(err);
        return ParseResult{options, false, EXIT_FAILURE};
    }

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc - 1));
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }
    parser.parse(toViews(arguments));

    if (help)
    {
        parser.printHelp(out);
        return ParseResult{options, false, EXIT_SUCCESS};
    }

    if (options.contextLines < 0)
    {
        err << "--context cannot be negative\n";
        return ParseResult{options, false, 2};
    }

    if (options.pdxFile.empty())
    {
        parser.printHelp(err);
        return ParseResult{options, false, 2};
    }

    options.mode = selectedReportMode(modeFlags);
    options.valueQueries = valueQueriesFrom(valueQueryTexts);

    if (options.interactive && options.valueQueries.empty())
    {
        err << "--interactive requires at least one --find-value query\n";
        return ParseResult{options, false, 2};
    }
    if (options.interactive && !options.output.empty())
    {
        err << "--interactive cannot be used with --output\n";
        return ParseResult{options, false, 2};
    }

    return ParseResult{options, true, EXIT_SUCCESS};
}

std::string toLower(std::string value)
{
    for (auto &c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::vector<std::string> splitLines(const std::string &text)
{
    std::vector<std::string> lines;
    std::string line;

    for (const char c : text)
    {
        if (c == '\n')
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            lines.push_back(line);
            line.clear();
            continue;
        }
        line.push_back(c);
    }

    if (!line.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

void writeRawSearchMatch(std::ostream &out,
                         const pdxinfo::RawDocument &document,
                         const std::vector<std::string> &lines,
                         std::size_t matchedLine,
                         int contextLines,
                         std::size_t matchNumber)
{
    const auto context = static_cast<std::size_t>(contextLines);
    const auto first = matchedLine > context ? matchedLine - context : 0;
    const auto last = std::min(lines.size() - 1, matchedLine + context);

    out << "Match " << matchNumber << ": " << document.sourceName << ':' << (matchedLine + 1) << '\n';
    for (std::size_t line = first; line <= last; ++line)
    {
        out << (line == matchedLine ? "> " : "  ") << (line + 1) << ": " << lines[line] << '\n';
    }
    out << '\n';
}

void writeRawSearch(std::ostream &out,
                    const std::vector<pdxinfo::RawDocument> &documents,
                    const std::string &query,
                    int contextLines)
{
    const auto needle = toLower(query);
    std::size_t matches = 0;

    for (const auto &document : documents)
    {
        const auto lines = splitLines(document.content);
        for (std::size_t line = 0; line < lines.size(); ++line)
        {
            if (toLower(lines[line]).find(needle) == std::string::npos)
            {
                continue;
            }

            ++matches;
            writeRawSearchMatch(out, document, lines, line, contextLines, matches);
        }
    }

    out << "Raw XML matches: " << matches << '\n';
}

void writeRequestedOutput(std::istream &in, std::ostream &out, const ProgramOptions &options)
{
    pdxinfo::PackageLoader loader;

    if (!options.valueQueries.empty())
    {
        const auto documents = loader.loadRawDocuments(options.pdxFile);
        if (options.interactive)
        {
            pdxinfo::writeInteractiveValueFindings(in, out, documents, options.valueQueries);
        }
        else
        {
            pdxinfo::writeValueFindings(out, documents, options.valueQueries);
        }
        return;
    }

    if (!options.searchQuery.empty())
    {
        writeRawSearch(out, loader.loadRawDocuments(options.pdxFile), options.searchQuery, options.contextLines);
        return;
    }

    const auto database = loader.load(options.pdxFile);
    pdxinfo::writeReport(out, database, options.mode);
}

int writeToFile(const ProgramOptions &options, std::ostream &status, std::ostream &err)
{
    std::ofstream file{std::filesystem::path(options.output)};
    if (!file)
    {
        err << "Cannot open output file: " << options.output << '\n';
        return 1;
    }

    writeRequestedOutput(std::cin, file, options);

    if (!file)
    {
        err << "Failed while writing output file: " << options.output << '\n';
        return 1;
    }

    status << "Report written to " << options.output << '\n';
    return EXIT_SUCCESS;
}

int runWithOptions(const ProgramOptions &options, std::ostream &out, std::ostream &err)
{
    if (!options.output.empty())
    {
        return writeToFile(options, out, err);
    }

    writeRequestedOutput(std::cin, out, options);
    return EXIT_SUCCESS;
}
}

int runPdxSearchTool(int argc, char **argv)
{
    try
    {
        const auto parsed = parseCommandLine(argc, argv, std::cout, std::cerr);
        if (!parsed.shouldRun)
        {
            return parsed.exitCode;
        }
        return runWithOptions(parsed.options, std::cout, std::cerr);
    }
    catch (const std::exception &error)
    {
        std::cerr << "pdx-search-tool: " << error.what() << '\n';
        return 1;
    }
}
