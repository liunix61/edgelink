#pragma once

namespace edgelink {

using JsonValue = boost::json::value;
using JsonString = boost::json::string;
using JsonObject = boost::json::object;
using JsonArray = boost::json::array;
using JsonKind = boost::json::kind;

class JsonProxy {

  public:
    explicit JsonProxy(JsonValue& jv) noexcept : jv_(jv) {}

    JsonProxy operator[](const std::string_view key) {
        JsonObject* obj;
        if (jv_.is_null())
            obj = &jv_.emplace_object();
        else
            obj = &jv_.as_object();
        return JsonProxy((*obj)[key]);
    }

    JsonProxy operator[](size_t index) {
        JsonArray* arr;
        if (jv_.is_null()) {

            arr = &jv_.emplace_array();
        } else {

            arr = &jv_.as_array();
        }
        if (arr->size() <= index) {
            arr->resize(index + 1);
        }
        return JsonProxy((*arr)[index]);
    }

    JsonValue& operator*() noexcept { return jv_; }

  private:
    JsonValue& jv_;
};

inline const std::string_view value_or(const JsonObject& json_obj, const std::string_view key,
                                       const std::string_view default_value) {
    if (auto val = json_obj.if_contains(key)) {
        return std::string_view(val->as_string());
    }
    return default_value;
}

/// @brief 从 JSON 对象获取指定值，如果找不到返回默认值
/// @param json_obj
/// @param key
/// @param default_value
/// @return
inline bool value_or(const JsonObject& json_obj, const std::string_view key, bool default_value) {
    if (auto val = json_obj.if_contains(key)) {
        if (val->is_string()) {
            auto str = val->as_string();
            return str == "true" ? true : false;
        } else {
            return val->as_bool();
        }
    }
    return default_value;
}

/// @brief 从 JSON 对象获取指定值，如果找不到返回默认值
/// @tparam TNumber
/// @param json_obj
/// @param key
/// @param default_value
/// @return
template <typename TNumber,
          typename = std::enable_if_t<std::is_same<TNumber, int>::value || std::is_same<TNumber, unsigned int>::value>>
inline TNumber value_or(const JsonObject& json_obj, const std::string_view key, TNumber default_value) {
    if (auto val = json_obj.if_contains(key)) {
        if (val->is_string()) {
            return boost::lexical_cast<TNumber>(val->as_string().c_str());
        } else {
            return val->to_number<TNumber>();
        }
    }
    return default_value;
}

}; // namespace edgelink
