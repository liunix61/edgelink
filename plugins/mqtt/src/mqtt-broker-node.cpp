#include <edgelink/plugin.hpp>
#include "mqtt.hpp"

#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>

using namespace edgelink;

namespace edgelink::plugins::mqtt {

/*
    {
        "id": "3c39cf63714c26a4",
        "type": "mqtt-broker",
        "name": "",
        "broker": "test.mosquitto.org",
        "port": "1883",
        "clientid": "1883",
        "autoConnect": true,
        "usetls": false,
        "protocolVersion": "4",
        "keepalive": "60",
        "cleansession": true,
        ...
    }
*/

/// @brief 去 Boost：MQTT broker 节点改用 EMQX NanoSDK（nng 纯 C 客户端，零 Boost）
class MqttBrokerNode : public EndpointNode,
                       public std::enable_shared_from_this<MqttBrokerNode>,
                       public IMqttBrokerEndpoint {
  public:
    MqttBrokerNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                   IEngine* engine)
        : EndpointNode(id, desc, config, engine, config.at("broker").get<std::string>(),
                       static_cast<uint16_t>(std::stoul(config.at("port").get<std::string>()))),
          _client_id(config.contains("clientid") ? config.at("clientid").get<std::string>() : "edgelink"),
          _keepalive(config.contains("keepalive") ? static_cast<uint16_t>(std::stoul(config.at("keepalive").get<std::string>()))
                                                  : 60) {
        //
    }

    ~MqttBrokerNode() {
        // 关闭 socket
        this->stop_recv_thread();
        if (_sock.id != 0) {
            nng_close(_sock);
            _sock.id = 0;
        }
    }

    Awaitable<void> async_start() override {
        this->logger()->info("MQTT BROKER > 启动连接：{}:{}", this->host(), this->port());
        // 打开 NanoSDK MQTT 客户端
        int rv = nng_mqtt_client_open(&_sock);
        if (rv != 0) {
            auto error_msg = fmt::format("nng_mqtt_client_open 失败：{0}", nng_strerror(rv));
            this->logger()->error(error_msg);
            throw IOException(error_msg);
        }

        // 构造 CONNECT 消息
        nng_msg* msg = nullptr;
        nng_mqtt_msg_alloc(&msg, 0);
        nng_mqtt_msg_set_packet_type(msg, NNG_MQTT_CONNECT);
        nng_mqtt_msg_set_connect_keep_alive(msg, _keepalive);
        nng_mqtt_msg_set_connect_proto_version(msg, MQTT_PROTOCOL_VERSION_v311);
        nng_mqtt_msg_set_connect_clean_session(msg, true);
        nng_mqtt_msg_set_connect_client_id(msg, _client_id.c_str());

        // 注册连接/断开回调（arg 传 this）
        nng_mqtt_set_connect_cb(_sock, MqttBrokerNode::connect_cb, this);
        nng_mqtt_set_disconnect_cb(_sock, MqttBrokerNode::disconnect_cb, this);

        // 构造 URL：nng://host:port（MQTT over TCP 默认）
        std::string url = "mqtt-tcp://" + std::string(this->host()) + ":" + std::to_string(this->port());

        // 创建 dialer 并携带 CONNECT 消息
        if ((rv = nng_dialer_create(&_dialer, _sock, url.c_str())) != 0) {
            auto error_msg = fmt::format("nng_dialer_create 失败：{0}", nng_strerror(rv));
            this->logger()->error(error_msg);
            nng_msg_free(msg);
            throw IOException(error_msg);
        }
        nng_dialer_set_ptr(_dialer, NNG_OPT_MQTT_CONNMSG, msg);
        if ((rv = nng_dialer_start(_dialer, NNG_FLAG_NONBLOCK)) != 0) {
            auto error_msg = fmt::format("nng_dialer_start 失败：{0}", nng_strerror(rv));
            this->logger()->error(error_msg);
            throw IOException(error_msg);
        }

        // 启动后台收消息线程
        this->start_recv_thread();

        co_return;
    }

    Awaitable<void> async_stop() override {
        this->logger()->info("MQTT BROKER > 停止连接");
        this->stop_recv_thread();
        if (_sock.id != 0) {
            nng_close(_sock);
            _sock.id = 0;
        }
        _connected.store(false);
        co_return;
    }

    bool is_connected() const override { return _connected.load(); }

    Awaitable<void> async_publish(const std::string_view topic, const uint8_t* payload,
                                  size_t payload_len, uint8_t qos) override {
        if (_sock.id == 0) {
            this->logger()->error("MQTT BROKER > 未连接，无法发布");
            co_return;
        }
        // 构造 PUBLISH 消息（NanoSDK 无 nng_mqtt_publish 便捷函数，需手造 msg）
        nng_msg* pub_msg = nullptr;
        if (nng_mqtt_msg_alloc(&pub_msg, 0) != 0) {
            this->logger()->error("MQTT BROKER > PUBLISH msg 分配失败");
            co_return;
        }
        nng_mqtt_msg_set_packet_type(pub_msg, NNG_MQTT_PUBLISH);
        nng_mqtt_msg_set_publish_topic(pub_msg, std::string(topic).c_str());
        nng_mqtt_msg_set_publish_payload(pub_msg, const_cast<uint8_t*>(payload), static_cast<uint32_t>(payload_len));
        nng_mqtt_msg_set_publish_qos(pub_msg, qos);
        nng_mqtt_msg_set_publish_retain(pub_msg, false);

        int rv = nng_sendmsg(_sock, pub_msg, NNG_FLAG_NONBLOCK);
        if (rv != 0) {
            this->logger()->error("MQTT BROKER > 发布失败：{0}", nng_strerror(rv));
            nng_msg_free(pub_msg);
        }
        co_return;
    }

    Awaitable<void> async_publish_string(const std::string_view topic, const std::string_view payload,
                                         uint8_t qos) override {
        co_await async_publish(topic, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), qos);
        co_return;
    }

    Awaitable<void> async_subscribe(const std::string_view topic, uint8_t qos) override {
        if (_sock.id == 0) {
            this->logger()->error("MQTT BROKER > 未连接，无法订阅");
            co_return;
        }
        // 构造 SUBSCRIBE 消息
        nng_msg* sub_msg = nullptr;
        if (nng_mqtt_msg_alloc(&sub_msg, 0) != 0) {
            this->logger()->error("MQTT BROKER > SUBSCRIBE msg 分配失败");
            co_return;
        }
        std::string topic_str(topic);
        nng_mqtt_topic_qos topic_qos;
        topic_qos.qos = qos;
        topic_qos.topic.buf = reinterpret_cast<uint8_t*>(const_cast<char*>(topic_str.data()));
        topic_qos.topic.length = static_cast<uint32_t>(topic_str.size());

        nng_mqtt_msg_set_packet_type(sub_msg, NNG_MQTT_SUBSCRIBE);
        nng_mqtt_msg_set_subscribe_topics(sub_msg, &topic_qos, 1);

        int rv = nng_sendmsg(_sock, sub_msg, NNG_FLAG_NONBLOCK);
        if (rv != 0) {
            this->logger()->error("MQTT BROKER > 订阅失败：{0}", nng_strerror(rv));
            nng_msg_free(sub_msg);
        } else {
            this->logger()->info("MQTT BROKER > 已订阅：{}", topic);
        }
        co_return;
    }

    void set_message_callback(void (*cb)(const std::string_view topic, const uint8_t* payload,
                                         size_t len, void* arg),
                              void* arg) override {
        _msg_cb = cb;
        _msg_arg = arg;
    }

    /// @brief 启动后台收消息线程（NanoSDK nng_recv 阻塞）
    void start_recv_thread() {
        if (_recv_thread_running.exchange(true)) {
            return;
        }
        _recv_thread = std::thread([this]() {
            for (;;) {
                nng_msg* msg = nullptr;
                size_t recv_len = 0;
                int rv = nng_recv(_sock, &msg, &recv_len, NNG_FLAG_NONBLOCK);
                if (rv == NNG_EAGAIN) {
                    nng_msleep(10);
                    continue;
                }
                if (rv != 0) {
                    if (rv == NNG_ECLOSED) {
                        break;
                    }
                    nng_msleep(50);
                    continue;
                }
                if (msg == nullptr) {
                    continue;
                }
                // 分发 PUBLISH 消息
                if (nng_mqtt_msg_get_packet_type(msg) == NNG_MQTT_PUBLISH && _msg_cb != nullptr) {
                    uint32_t topic_len = 0, payload_len = 0;
                    const char* topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);
                    uint8_t* payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
                    std::string topic_str(topic ? topic : "", topic_len);
                    _msg_cb(topic_str, payload, payload_len, _msg_arg);
                }
                nng_msg_free(msg);
            }
        });
    }

    void stop_recv_thread() {
        _recv_thread_running.store(false);
        if (_recv_thread.joinable()) {
            _recv_thread.join();
        }
    }

  private:
    /// @brief 连接成功回调（NanoSDK 内部线程调用）——发布订阅消息
    static void connect_cb(nng_pipe pipe, nng_pipe_ev ev, void* arg) {
        (void)pipe;
        (void)ev;
        auto* self = static_cast<MqttBrokerNode*>(arg);
        self->_connected.store(true);
        self->logger()->info("MQTT BROKER > 已连接：{}:{}", self->host(), self->port());
    }

    /// @brief 断开回调
    static void disconnect_cb(nng_pipe pipe, nng_pipe_ev ev, void* arg) {
        (void)pipe;
        (void)ev;
        auto* self = static_cast<MqttBrokerNode*>(arg);
        self->_connected.store(false);
        self->logger()->info("MQTT BROKER > 已断开：{}:{}", self->host(), self->port());
    }

  private:
    nng_socket _sock{};
    nng_dialer _dialer{};
    std::atomic<bool> _connected{false};
    std::string _client_id;
    uint16_t _keepalive;

    // 消息回调（in 节点注册）与后台收线程
    void (*_msg_cb)(const std::string_view topic, const uint8_t* payload, size_t len, void* arg) = nullptr;
    void* _msg_arg = nullptr;
    std::thread _recv_thread;
    std::atomic<bool> _recv_thread_running{false};
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<StandaloneNodeProvider<MqttBrokerNode, "mqtt-broker", NodeKind::STANDALONE>>(
        "edgelink::plugins::mqtt::MqttBrokerNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};

}; // namespace edgelink::plugins::mqtt
