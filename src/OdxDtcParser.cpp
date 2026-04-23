#include "pdxinfo/OdxDtcParser.hpp"

namespace pdxinfo::detail {

OdxDtcParser::OdxDtcParser(const OdxNodeReader& reader)
    : reader_(reader) {
}

Dtc OdxDtcParser::parseDtc(const xml::Node& node) const {
    Dtc dtc;
    dtc.id = reader_.nodeId(node);
    dtc.shortName = reader_.shortNameOf(node);
    dtc.codedValue = reader_.parseInteger(node.childTextLocal("CODED-VALUE"));
    dtc.troubleCode = node.childTextLocal("DISPLAY-TROUBLE-CODE");
    if (dtc.troubleCode.empty()) {
        dtc.troubleCode = node.childTextLocal("TROUBLE-CODE");
    }
    dtc.text = node.childTextLocal("TEXT");
    if (dtc.text.empty()) {
        dtc.text = node.childTextLocal("DESC");
    }
    return dtc;
}

}
