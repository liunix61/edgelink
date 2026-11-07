#include <edgelink/plugin.hpp>
#include "mqtt.hpp"

#include <string>

using namespace edgelink;

namespace edgelink::plugins::mqtt {

/*
    {
        "id": "f83cdc7c9c540aa8",
        "type": "mqtt out",
        "z": "7c226c13f2e3b224",
        "name": "",
        "topic": "",
        "qos": "",
        "retain": "",
        "respTopic": "",
        "contentType": "",
        "userProps": "",
        "correl": "",
        "expiry": "",
        "broker": "",
        "x": 870,
        "y": 320,
        "wires": []
    }
*/

/// @brief 去 Boost：MQTT out 节点改用 NanoSDK（纯 C 零 Boost）
class MqttOutNode : public SinkNode, public std::enable_shared_from_this<MqttOutNode> {
  public:
    MqttOutNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : SinkNode(id, desc, flow, config), _mqtt_broker_node_id(config.at("broker").get<std::string>()) {
        try {
            if (config.contains("topic")) {
                const std::string topic_str = config.at("topic").get<std::string>();
                if (!topic_str.empty()) {
                    _node_topic = topic_str;
                }
            }

            if (config.contains("qos")) {
                const std::string qos_str = config.at("qos").get<std::string>();
                if (qos_str == "0" || qos_str == "1" || qos_str == "2") {
                    _node_qos = static_cast<uint8_t>(std::stoul(qos_str));
                }
            }
        } catch (std::exception& ex) {
            this->logger()->error("加载 MQTT Out 节点配置发生错误：{0}", ex.what());
            throw;
        }
    }

    Awaitable<void> async_start() override {
        this->logger()->info("MQTT OUT > 启动");
        co_return;
    }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {

        if (!msg->data().contains("payload")) {
            co_return;
        }

        auto topic = _node_topic.has_value() ? std::string_view(*_node_topic)
                                             : std::string_view(msg->data().at("topic").get<std::string>());
        auto qos = _node_qos.has_value() ? *_node_qos
                                         : static_cast<uint8_t>(msg->data().at("qos").get<int>());

        auto json_payload_value = msg->data().at("payload");

        auto mqtt_node = this->flow()->engine()->get_global_node(_mqtt_broker_node_id);
        auto mqtt = dynamic_cast<IMqttBrokerEndpoint*>(mqtt_node);
        if (mqtt == nullptr) {
            auto error_msg = "参数指定的 broker 节点不是 'mqtt-broker' 节点类型";
            this->logger()->error(error_msg);
            throw InvalidDataException(error_msg);
        }

        if (json_payload_value.is_array()) { // 是数组就假定要发送的是字节数组
            std::vector<char> bytes(json_payload_value.size());
            for (size_t i = 0; i < json_payload_value.size(); i++) {
                bytes[i] = json_payload_value.at(i).get<char>();
            }
            co_await mqtt->async_publish(topic, reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), qos);
        } else if (json_payload_value.is_string()) { // 字符串原样发送
            auto s = json_payload_value.get<std::string>();
            co_await mqtt->async_publish(topic, reinterpret_cast<const uint8_t*>(s.data()), s.size(), qos);
        } else if (json_payload_value.is_object() || json_payload_value.is_boolean() ||
                   json_payload_value.is_number_integer() || json_payload_value.is_number_unsigned() ||
                   json_payload_value.is_number_float()) { // 对象和其他数字、布尔值转换为 JSON 字符串发送
            auto payload_text = json_payload_value.dump();
            co_await mqtt->async_publish(topic, reinterpret_cast<const uint8_t*>(payload_text.data()),
                                         payload_text.size(), qos);
        } else {
            auto error_msg = fmt::format("'mqtt out' 节点不支持负载：'{0}'", json_payload_value.dump());
            SPDLOG_ERROR(error_msg);
        }

        co_return;
    }

  private:
    std::string _mqtt_broker_node_id;
    std::optional<std::string> _node_topic;
    std::optional<uint8_t> _node_qos;
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<MqttOutNode, "mqtt out", NodeKind::SINK>>(
        "edgelink::plugins::mqtt::MqttOutNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};

}; // namespace edgelink::plugins::mqtt
