#pragma once

#include "edgelink/json.hpp"

namespace edgelink {

using FlowElementID = unsigned int;

class EDGELINK_EXPORT FlowConfig {
  public:
    FlowConfig(const JsonArray& json_array) : _json_array(json_array) {}
    FlowConfig(const FlowConfig& other) : _json_array(other._json_array) {}
    FlowConfig(const FlowConfig&& other) : _json_array(std::move(other._json_array)) {}

    const JsonArray data() const { return _json_array; }

  private:
    JsonArray _json_array;
};

}; // namespace edgelink
