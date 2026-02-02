#include "edgelink/edgelink.hpp"
#include "edgelink/prop-tuple.hpp"

namespace edgelink {

PropValueType tag_invoke(boost::json::value_to_tag<PropValueType>, JsonValue const& jv) {
    static std::unordered_map<JsonString, PropValueType> mapping({
        {"str", PropValueType::STR},
        {"num", PropValueType::NUM},
        {"json", PropValueType::JSON},
        {"re", PropValueType::RE},
        {"date", PropValueType::DATE},
        {"bin", PropValueType::BIN},
        {"msg", PropValueType::MSG},
        {"flow", PropValueType::FLOW},
        {"global", PropValueType::GLOBAL},
        {"bool", PropValueType::BOOL},
        {"jsonata", PropValueType::JSONATA},
        {"env", PropValueType::ENV},
    });

    return mapping.at(jv.as_string());
}

PropTuple tag_invoke(boost::json::value_to_tag<PropTuple>, JsonValue const& jv) {
    auto const& obj = jv.as_object();
    auto name = boost::json::value_to<std::string>(obj.at("p"));
    auto type = boost::json::value_to<PropValueType>(obj.at("vt"));
    return PropTuple{
        .name = std::move(name),
        .type = std::move(type),
        .value = std::move(Variant()),
    };
}

std::vector<PropTuple> from_json(const JsonValue& jv) {
    std::vector<PropTuple> vec;
    auto const& jarray = jv.as_array();
    for (auto const& jitem : jarray) {
        vec.emplace_back(std::move(boost::json::value_to<PropTuple>(jitem)));
    }
    return vec;
}

}; // namespace edgelink