#include "pdxinfo/Report.hpp"

#include "pdxinfo/Bytes.hpp"

#include <iostream>

namespace pdxinfo {
namespace {

struct Totals {
    std::size_t ecus = 0;
    std::size_t services = 0;
    std::size_t dtcs = 0;
    std::size_t dids = 0;
};

Totals totalsOf(const DiagnosticDatabase& database) {
    Totals totals;
    for (const auto& document : database.documents) {
        totals.ecus += document.ecus.size();
        totals.services += document.looseServices.size();
        totals.dtcs += document.dtcs.size();
        totals.dids += document.dids.size();
    }
    return totals;
}

void writeParameter(std::ostream& out, const Parameter& parameter, const std::string& indent) {
    out << indent << "- " << (parameter.shortName.empty() ? "<unnamed>" : parameter.shortName);
    if (!parameter.semantic.empty()) {
        out << " [" << parameter.semantic << "]";
    }
    if (parameter.bytePosition) {
        out << " byte=" << *parameter.bytePosition;
    }
    if (parameter.codedValue) {
        out << " value=" << toHex(*parameter.codedValue);
    }
    if (!parameter.dopRef.empty()) {
        out << " dop-ref=" << parameter.dopRef;
    }
    if (!parameter.tableRef.empty()) {
        out << " table-ref=" << parameter.tableRef;
    }
    out << '\n';
}

void writeMessage(std::ostream& out, const Message& message, const std::string& title, const std::string& indent) {
    out << indent << title << ": " << (message.shortName.empty() ? "<unnamed>" : message.shortName);
    if (!message.id.empty()) {
        out << " (" << message.id << ")";
    }
    out << '\n';

    for (const auto& parameter : message.parameters) {
        writeParameter(out, parameter, indent + "  ");
    }
}

void writeService(std::ostream& out, const DiagnosticService& service, const std::string& indent) {
    out << indent << "- " << (service.shortName.empty() ? "<unnamed service>" : service.shortName);
    if (!service.semantic.empty()) {
        out << " [" << service.semantic << "]";
    }
    if (service.requestSid) {
        out << " requestSID=" << toHex(*service.requestSid, 2);
    }
    if (service.positiveResponseSid) {
        out << " positiveSID=" << toHex(*service.positiveResponseSid, 2);
    }
    out << '\n';

    for (const auto& request : service.requests) {
        writeMessage(out, request, "Request", indent + "  ");
    }
    for (const auto& response : service.positiveResponses) {
        writeMessage(out, response, "Positive response", indent + "  ");
    }
    for (const auto& response : service.negativeResponses) {
        writeMessage(out, response, "Negative response", indent + "  ");
    }
}

void writeSummary(std::ostream& out, const DiagnosticDatabase& database) {
    const auto totals = totalsOf(database);
    out << "PDX/ODX Diagnostic Summary\n";
    out << "Documents: " << database.documents.size() << '\n';
    out << "ECUs:      " << totals.ecus << '\n';
    out << "Services:  " << totals.services << '\n';
    out << "DTCs:      " << totals.dtcs << '\n';
    out << "DIDs:      " << totals.dids << "\n\n";

    for (const auto& document : database.documents) {
        out << "Document: " << document.sourceName << '\n';
        out << "  ECUs: " << document.ecus.size()
            << ", services: " << document.looseServices.size()
            << ", DTCs: " << document.dtcs.size()
            << ", DIDs: " << document.dids.size() << '\n';
    }
}

void writeServices(std::ostream& out, const OdxDocument& document) {
    for (const auto& ecu : document.ecus) {
        out << "ECU: " << (ecu.shortName.empty() ? "<unnamed>" : ecu.shortName);
        if (!ecu.type.empty()) {
            out << " [" << ecu.type << "]";
        }
        if (!ecu.id.empty()) {
            out << " (" << ecu.id << ")";
        }
        out << '\n';

        if (ecu.services.empty()) {
            out << "  No diagnostic services found directly under this ECU layer.\n";
        }
        for (const auto& service : ecu.services) {
            writeService(out, service, "  ");
        }
        out << '\n';
    }

    if (document.ecus.empty() && !document.looseServices.empty()) {
        out << "Services:\n";
        for (const auto& service : document.looseServices) {
            writeService(out, service, "  ");
        }
    }
}

void writeDtcs(std::ostream& out, const OdxDocument& document) {
    if (document.dtcs.empty()) {
        out << "No DTC definitions found.\n";
        return;
    }

    for (const auto& dtc : document.dtcs) {
        out << "- " << (dtc.shortName.empty() ? "<unnamed DTC>" : dtc.shortName);
        if (!dtc.troubleCode.empty()) {
            out << " code=" << dtc.troubleCode;
        }
        if (dtc.codedValue) {
            out << " coded=" << toHex(*dtc.codedValue);
        }
        if (!dtc.text.empty()) {
            out << " - " << dtc.text;
        }
        out << '\n';
    }
}

void writeDids(std::ostream& out, const OdxDocument& document) {
    if (document.dids.empty()) {
        out << "No DID candidates found.\n";
        return;
    }

    for (const auto& did : document.dids) {
        out << "- " << (did.shortName.empty() ? "<unnamed DID>" : did.shortName);
        if (did.identifier) {
            out << " id=" << toHex(*did.identifier, 4);
        }
        if (!did.sourceService.empty()) {
            out << " source=" << did.sourceService;
        }
        out << '\n';
    }
}

}

void writeReport(std::ostream& out, const DiagnosticDatabase& database, ReportMode mode) {
    if (mode == ReportMode::Summary) {
        writeSummary(out, database);
        return;
    }

    if (mode == ReportMode::Full) {
        writeSummary(out, database);
        out << "\n";
    }

    for (const auto& document : database.documents) {
        out << "Document: " << document.sourceName << "\n";

        if (mode == ReportMode::Full || mode == ReportMode::Services) {
            out << "\nServices\n";
            writeServices(out, document);
        }

        if (mode == ReportMode::Full || mode == ReportMode::Dtcs) {
            out << "\nDTCs\n";
            writeDtcs(out, document);
        }

        if (mode == ReportMode::Full || mode == ReportMode::Dids) {
            out << "\nDIDs\n";
            writeDids(out, document);
        }

        out << '\n';
    }
}

}

