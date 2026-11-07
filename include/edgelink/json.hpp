#pragma once

// edgelink JSON 封装（替代 boost::json，用 nlohmann/json 零 Boost）
// 保持原 API 兼容：JsonValue/JsonString/JsonObject/JsonArray/JsonKind + JsonProxy + value_or

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <type_traits>

namespace edgelink {

using JsonValue = nlohmann::json;
using JsonString = std::string;
using JsonObject = nlohmann::json;
using JsonArray = nlohmann::json;

// boost::json::kind 兼容枚举
enum class JsonKind {
    null_,
    boolean_,
    number_,
    string_,
    array_,
    object_,
};

/// @brief 从 JsonValue 提取 kind（兼容 boost::json::kind 语义）
inline JsonKind json_kind_of(const JsonValue& v) {
    if (v.is_null()) return JsonKind::null_;
    if (v.is_boolean()) return JsonKind::boolean_;
    if (v.is_number()) return JsonKind::number_;
    if (v.is_string()) return JsonKind::string_;
    if (v.is_array()) return JsonKind::array_;
    return JsonKind::object_;
}

class JsonProxy {

  public:
    explicit JsonProxy(JsonValue& jv) noexcept : jv_(jv) {}

    JsonProxy operator[](const std::string_view key) {
        if (!jv_.is_object()) {
            jv_ = JsonObject::object();
        }
        JsonObject& obj = jv_;
        return JsonProxy(obj[std::string(key)]);
    }

    JsonProxy operator[](size_t index) {
        if (!jv_.is_array()) {
            jv_ = JsonArray::array();
        }
        JsonArray& arr = jv_;
        if (arr.size() <= index) {
            // nlohmann 数组用 null 填充到 index
            while (arr.size() <= index) {
                arr.push_back(nullptr);
            }
        }
        return JsonProxy(arr[index]);
    }

    JsonValue& operator*() noexcept { return jv_; }

  private:
    JsonValue& jv_;
};

/// @brief 从 JSON 对象获取指定字符串值，如果找不到返回默认值
inline const std::string_view value_or(const JsonObject& json_obj, const std::string_view key,
                                       const std::string_view default_value) {
    auto it = json_obj.find(std::string(key));
    if (it != json_obj.end() && it->is_string()) {
        return std::string_view(it->get<std::string>());
    }
    return default_value;
}

/// @brief 从 JSON 对象获取指定 bool 值，如果找不到返回默认值
inline bool value_or(const JsonObject& json_obj, const std::string_view key, bool default_value) {
    auto it = json_obj.find(std::string(key));
    if (it != json_obj.end()) {
        if (it->is_string()) {
            auto str = it->get<std::string>();
            return str == "true" ? true : false;
        } else if (it->is_boolean()) {
            return it->get<bool>();
        }
    }
    return default_value;
}

/// @brief 从 JSON 对象获取指定数值，如果找不到返回默认值
template <typename TNumber,
          typename = std::enable_if_t<std::is_same<TNumber, int>::value || std::is_same<TNumber, unsigned int>::value>>
inline TNumber value_or(const JsonObject& json_obj, const std::string_view key, TNumber default_value) {
    auto it = json_obj.find(std::string(key));
    if (it != json_obj.end()) {
        if (it->is_string()) {
            // 字符串数字转数值（兼容 boost::lexical_cast）
            std::string s = it->get<std::string>();
            return static_cast<TNumber>(std::stoll(s));
        } else if (it->is_number()) {
            return it->get<TNumber>();
        }
    }
    return default_value;
}

}; // namespace edgelink
