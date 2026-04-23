#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pdxinfo {

struct Parameter {
    std::string shortName;
    std::string semantic;
    std::optional<int> bytePosition;
    std::optional<std::uint64_t> codedValue;
    std::string dopRef;
    std::string tableRef;
    std::string rawElement;
};

struct Message {
    std::string id;
    std::string shortName;
    std::vector<Parameter> parameters;
};

struct DiagnosticService {
    std::string id;
    std::string shortName;
    std::string semantic;
    std::optional<std::uint64_t> requestSid;
    std::optional<std::uint64_t> positiveResponseSid;
    std::vector<Message> requests;
    std::vector<Message> positiveResponses;
    std::vector<Message> negativeResponses;
};

/*```xml
<DTC ID="DTCDOP_VAGUDS.DTC_2857">
  <SHORT-NAME>DTC_2857</SHORT-NAME>
  <TROUBLE-CODE>2857</TROUBLE-CODE>
  <DISPLAY-TROUBLE-CODE>P003800</DISPLAY-TROUBLE-CODE>
  <TEXT TI="P003800">O2 Sensor Heater Contr. Circ.(Bank1(1)Sensor 2)
High</TEXT>
  <LEVEL>2</LEVEL>
</DTC>
```
*/
struct Dtc {
    std::string id;
    std::string shortName;
    std::optional<std::uint64_t> codedValue;
    std::string troubleCode;
    std::string text;
};

struct Did {
    std::string shortName;
    std::optional<std::uint64_t> identifier;
    std::string sourceService;
};

struct Ecu {
    std::string id;
    std::string shortName;
    std::string type;
    std::vector<DiagnosticService> services;
};

struct OdxDocument {
    std::string sourceName;
    std::vector<Ecu> ecus;
    std::vector<DiagnosticService> looseServices;
    std::vector<Dtc> dtcs;
    std::vector<Did> dids;
};

struct DiagnosticDatabase {
    std::vector<OdxDocument> documents;
};

}

