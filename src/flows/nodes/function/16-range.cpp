#include "edgelink/edgelink.hpp"

using namespace edgelink;

/*
    {
    "id": "b1c267019d45655a",
    "type": "range",
    "z": "7c226c13f2e3b224",
    "minin": "12",
    "maxin": "12",
    "minout": "11",
    "maxout": "11",
    "action": "scale",
    "round": true,
    "property": "payload",
    "name": "",
    "x": 390,
    "y": 480,
    "wires": [
        []
    ]
}

*/

static double parse_json_number(const JsonObject& config, const std::string_view prop) {
    const auto& str_value = config.at(prop).as_string();
    if (str_value.empty()) {
        return NAN;
    }
    return boost::lexical_cast<double>(str_value.c_str());
}

class RangeNode : public FlowNode {
  public:
    RangeNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config),             //
          _minin(parse_json_number(config, "minin")),   //
          _maxin(parse_json_number(config, "maxin")),   //
          _minout(parse_json_number(config, "minout")), //
          _maxout(parse_json_number(config, "maxout")), //
          _action(config.at("action").as_string()),     //
          _round(config.at("round").as_bool()),         //
          _property(config.at("property").as_string())  //
    {
        //
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        if (std::isnan(_minin) || std::isnan(_maxin) || std::isnan(_minout) || std::isnan(_maxout)) {
            co_return;
        }

        auto value = msg->at_propex(_property);
        if (value.is_number()) {
            double n = value.to_number<double>();
            if (_action == "clamp") {
                if (n < _minin) {
                    n = _minin;
                }
                if (n > _maxin) {
                    n = _maxin;
                }
            }
            if (_action == "roll") {
                auto divisor = _maxin - _minin;
                n = std::fmod(std::fmod(n - _minin, divisor + divisor), divisor) + _minin;
            }
            n = ((n - _minin) / (_maxin - _minin) * (_maxout - _minout)) + _minout;
            if (_round) {
                n = static_cast<int64_t>(std::round(n));
            }
            msg->at_propex(_property) = n;
            co_await this->async_send_to_one_port(msg);
        }
        co_return;
    }

  private:
    double _minin;
    double _maxin;
    double _minout;
    double _maxout;
    const std::string _action;
    bool _round;
    const std::string _property;
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<RangeNode, "range", NodeKind::PIPE>>("edgelink::RangeNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};