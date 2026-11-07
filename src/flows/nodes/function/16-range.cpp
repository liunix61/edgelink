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
    const std::string str_value = config.at(prop).get<std::string>();
    if (str_value.empty()) {
        return NAN;
    }
    return std::stod(str_value);
}

class RangeNode : public FlowNode {
  public:
    RangeNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config),             //
          _minin(parse_json_number(config, "minin")),   //
          _maxin(parse_json_number(config, "maxin")),   //
          _minout(parse_json_number(config, "minout")), //
          _maxout(parse_json_number(config, "maxout")), //
          _action(config.at("action").get<std::string>()),     //
          _round(config.at("round").get<bool>()),         //
          _property(config.at("property").get<std::string>())  //
    {
        //
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        if (std::isnan(_minin) || std::isnan(_maxin) || std::isnan(_minout) || std::isnan(_maxout)) {
            co_return;
        }

        // 修复：除数 _maxin - _minin 为 0 时原计算除零（NaN/Inf）——跳过
        if (_maxin == _minin) {
            this->logger()->warn("range 节点 minin==maxin（{0}），除零，跳过", _minin);
            co_return;
        }

        JsonValue value;
        try {
            value = msg->at_propex(_property);
        } catch (const std::exception& ex) {
            // 修复：属性不存在时原 at_propex 抛异常未捕获——记录并跳过
            this->logger()->warn("range 节点属性 {0} 不存在：{1}", _property, ex.what());
            co_return;
        }
        if (value.is_number()) {
            double n = value.get<double>();
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
                if (divisor != 0) {
                    n = std::fmod(std::fmod(n - _minin, divisor + divisor), divisor) + _minin;
                }
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