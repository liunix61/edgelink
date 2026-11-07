#include <edgelink/plugin.hpp>
#include "mqtt.hpp"

#include <atomic>
#include <string>
#include <vector>

using namespace edgelink;
namespace this_coro = edgelink::this_coro;

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

/// @brief 去 Boost：MQTT in 节点改用 NanoSDK（纯 C 零 Boost）
class MqttInNode : public SourceNode, public std::enable_shared_from_this<MqttInNode> {
  public:
    MqttInNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : SourceNode(id, desc, flow, config), _mqtt_broker_node_id(config.at("broker").get<std::string>()),
          _topic(config.at("topic").get<std::string>()),
          _qos(static_cast<uint8_t>(std::stoul(config.at("qos").get<std::string>()))),
          _data_type(config.at("datatype").get<std::string>()), _nl(config.at("nl").get<bool>()),
          _rap(config.at("rap").get<bool>()), _rh(config.at("rh").get<int>()),
          _inputs(config.at("input").get<size_t>()) {
        //
    }

  protected:
    Awaitable<void> on_async_run() override {
        // 获取 broker 节点
        auto mqtt_node = this->flow()->engine()->get_global_node(_mqtt_broker_node_id);
        auto mqtt = dynamic_cast<IMqttBrokerEndpoint*>(mqtt_node);
        if (mqtt == nullptr) {
            this->logger()->error("'mqtt in' 节点指定的 broker 不存在或不是 mqtt-broker 类型");
            co_return;
        }

        // 保存 executor（回调跨线程投递用）
        _executor = co_await this_coro::executor();
        _executor_valid = true;

        // 注册消息回调（broker 收线程调用）
        mqtt->set_message_callback(&MqttInNode::on_message_cb, this);

        // 订阅主题
        co_await mqtt->async_subscribe(_topic, _qos);
        this->logger()->info("MQTT IN > 已订阅：{}", _topic);

        co_return;
    }

    /// @brief 消息回调（broker 收线程调用）——投递到协程发流
    static void on_message_cb(const std::string_view topic, const uint8_t* payload, size_t len, void* arg) {
        auto* self = static_cast<MqttInNode*>(arg);
        self->dispatch_message(topic, payload, len);
    }

    /// @brief 投递消息到协程（经 executor.post）
    void dispatch_message(const std::string_view topic, const uint8_t* payload, size_t len) {
        std::string topic_str(topic);
        std::vector<uint8_t> payload_vec(payload, payload + len);
        if (_executor_valid) {
            _executor.post([self = this->shared_from_this(), topic_str, payload_vec]() {
                self->send_to_flow(topic_str, payload_vec);
            });
        }
    }

    /// @brief 构造 Msg 并发送到下游（在协程线程执行）
    void send_to_flow(const std::string& topic, const std::vector<uint8_t>& payload) {
        // 构造消息数据
        JsonObject data = JsonObject::object();
        data["topic"] = topic;
        data["payload"] = std::string(payload.begin(), payload.end());

        auto msg = std::make_shared<Msg>(std::move(data));
        auto self = this->shared_from_this();
        co_spawn(_executor, [self, msg]() -> Awaitable<void> {
            co_await self->async_send_to_one_port(std::move(msg));
            co_return;
        }(), Detached{});
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
    Executor _executor;
    bool _executor_valid = false;
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<MqttInNode, "mqtt in", NodeKind::SOURCE>>(
        "edgelink::plugins::mqtt::MqttInNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};

}; // namespace edgelink::plugins::mqtt
