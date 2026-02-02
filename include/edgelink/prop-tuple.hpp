#pragma once

namespace edgelink {

enum class PropValueType {
    STR = 0,
    NUM,
    JSON,
    RE,
    DATE,
    BIN,
    MSG,
    FLOW,
    GLOBAL,
    BOOL,
    JSONATA,
    ENV,
};

struct PropTuple {
    std::string name;
    PropValueType type;
    Variant value;
};

EDGELINK_EXPORT std::vector<PropTuple> from_json(const JsonValue& jv);

}; // namespace edgelink