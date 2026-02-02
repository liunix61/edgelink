#include <edgelink/plugin.hpp>
#include "mqtt.hpp"

using namespace edgelink;

namespace edgelink::plugins::mqtt {

/*
    {
        "id": "72be09264971c5ec",
        "type": "mqtt in",
        "z": "73e0fcd142fc5256",
        "name": "",
        "topic": "/yncic-dangerous/data/test1",
        "qos": "0",
        "datatype": "auto-detect",
        "broker": "3c39cf63714c26a4",
        "nl": false,
        "rap": true,
        "rh": 0,
        "inputs": 0,
        "x": 360,
        "y": 380,
        "wires": [
            [
                "adde85bf75a42c9c"
            ]
        ]
    },
*/

class MqttInNode : public SourceNode, public std::enable_shared_from_this<MqttInNode> {
  public:
    MqttInNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : SourceNode(id, desc, flow, config), _mqtt_broker_node_id(config.at("broker").as_string()),
          _topic(config.at("topic").as_string()),
          _qos(boost::lexical_cast<uint8_t>(config.at("qos").as_string().c_str())),
          _data_type(config.at("datatype").as_string()), _nl(config.at("nl").as_bool()),
          _rap(config.at("rap").as_bool()), _rh(config.at("rh").to_number<int>()),
          _inputs(config.at("input").to_number<size_t>()) {
        //
    }

  protected:
    Awaitable<void> on_async_run() override {
        //
        co_return;
    }

  private:
    std::string _mqtt_broker_node_id;
    const std::string _topic;
    const uint8_t _qos;
    const std::string _data_type;
    const bool _nl;
    const bool _rap;
    const int _rh;
    const size_t _inputs;
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<MqttInNode, "mqtt in", NodeKind::SOURCE>>(
        "edgelink::plugins::mqtt::MqttInNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};

}; // namespace edgelink::plugins::mqtt