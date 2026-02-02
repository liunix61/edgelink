#pragma once

namespace edgelink {

using FlowElementID = unsigned int;

class EDGELINK_EXPORT FlowConfig {
  public:
    FlowConfig(const boost::json::array& json_array) : _json_array(json_array) {}
    FlowConfig(const FlowConfig& other) : _json_array(other._json_array) {}
    FlowConfig(const FlowConfig&& other) : _json_array(std::move(other._json_array)) {}

    const boost::json::array data() const { return _json_array; }

  private:
    boost::json::array _json_array;
};

}; // namespace edgelink