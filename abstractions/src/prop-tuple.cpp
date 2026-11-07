#include "edgelink/edgelink.hpp"
#include "edgelink/prop-tuple.hpp"

namespace edgelink {

// nlohmann 替代 boost::json tag_invoke/value_to——手动转换
PropValueType prop_value_type_from_json(const JsonValue& jv) {
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

    std::string type_str = jv.is_string() ? jv.get<std::string>() : "";
    auto it = mapping.find(type_str);
    if (it != mapping.end()) {
        return it->second;
    }
    return PropValueType::STR;
}

PropTuple prop_tuple_from_json(const JsonValue& jv) {
    // 修复：`jv.is_object() ? jv : JsonValue::object()` ternary 临时 → obj 悬垂（:33 obj.value 已悬垂）。
    //       改为 if-early-continue（非 object 返回空 tuple），保证 obj 引用稳定的 jv。
    if (!jv.is_object()) {
        return PropTuple{};
    }
    auto const& obj = jv;
    auto name = obj.value("p", std::string());
    auto type = prop_value_type_from_json(obj.value("vt", JsonValue()));
    // 修复：原实现 .value = Variant()（恒 null），完全忽略 v 字段——导致 inject 等节点属性值丢失
    JsonValue value = nullptr;
    auto it = obj.find("v");
    if (it != obj.end()) {
        value = *it;
    }
    return PropTuple{
        .name = std::move(name),
        .type = std::move(type),
        .value = std::move(value),
    };
}

std::vector<PropTuple> from_json(const JsonValue& jv) {
    std::vector<PropTuple> vec;
    if (!jv.is_array()) {
        return vec;
    }
    for (auto const& jitem : jv) {
        vec.emplace_back(std::move(prop_tuple_from_json(jitem)));
    }
    return vec;
}

}; // namespace edgelink
