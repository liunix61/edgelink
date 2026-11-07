#pragma once

#include "edgelink/json.hpp"
#include <string>
#include <vector>

namespace edgelink {

using Variant = JsonValue;

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

EDGELINK_EXPORT PropValueType prop_value_type_from_json(const JsonValue& jv);
EDGELINK_EXPORT PropTuple prop_tuple_from_json(const JsonValue& jv);
EDGELINK_EXPORT std::vector<PropTuple> from_json(const JsonValue& jv);

}; // namespace edgelink
