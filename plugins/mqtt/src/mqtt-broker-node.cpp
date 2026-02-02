#include <boost/asio/experimental/channel.hpp>

#include <edgelink/plugin.hpp>
#include "mqtt.hpp"

using namespace edgelink;

namespace edgelink::plugins::mqtt {

using boost::asio::deferred;
using boost::asio::experimental::channel;
using namespace boost;
namespace asio = boost::asio;
namespace am = async_mqtt;
namespace this_coro = boost::asio::this_coro;
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
        "birthTopic": "",
        "birthQos": "0",
        "birthPayload": "",
        "birthMsg": {},
        "closeTopic": "",
        "closeQos": "0",
        "closePayload": "",
        "closeMsg": {},
        "willTopic": "",
        "willQos": "0",
        "willPayload": "",
        "willMsg": {},
        "userProps": "",
        "sessionExpiry": ""
    }
*/

class MqttBrokerNode : public EndpointNode,
                       public std::enable_shared_from_this<MqttBrokerNode>,
                       public IMqttBrokerEndpoint {
  public:
    MqttBrokerNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                   IEngine* engine)
        : EndpointNode(id, desc, config, engine, config.at("broker").as_string(),
                       boost::lexical_cast<uint16_t>(config.at("port").as_string().c_str())) {
        //
    }

    Awaitable<void> async_start() override {
        auto exe = co_await this_coro::executor;
        if (!_lock) { // 因 MQTT 库的限制，此类不运行并发操作
            _lock = std::make_unique<channel<void()>>(exe, 1);
        }
        else {
            throw std::logic_error("MqttBrokerNode 已启动，不能再次启动");
        }

        co_await this->async_lock();
        try {
            co_await this->async_connect();
        } catch (std::exception& ex) {
            auto error_msg = fmt::format("MQTT 连接失败：{0}", ex.what());
            this->logger()->error(error_msg);
            this->unlock();
            throw;
        }
        this->unlock();

        co_return;
    }

    Awaitable<void> async_stop() override {

        co_await this->async_close();
        _lock->cancel();
        _lock.reset();

        co_return;
    }

    bool is_connected() const override { return _endpoint && _endpoint->next_layer().is_open(); }

    Awaitable<void> async_publish(const std::string_view topic, const async_mqtt::buffer& payload_buffer,
                                  async_mqtt::qos qos) override {
        co_await this->async_lock();
        try {

            auto topic_buffer = am::allocate_buffer(topic);
            auto pid = co_await _endpoint->acquire_unique_packet_id(asio::use_awaitable);
            // Send MQTT PUBLISH
            auto se = co_await _endpoint->send(am::v3_1_1::publish_packet{*pid, topic_buffer, payload_buffer, qos},
                                               asio::use_awaitable);
            if (se) {
                auto error_msg = fmt::format("MQTT PUBLISH send error: {0}", se.what());
                this->logger()->error(error_msg);
        this->unlock();
                throw IOException(error_msg);
            }
            // Recv MQTT PUBLISH and PUBACK (order depends on broker)
            am::packet_variant pv = co_await _endpoint->recv(asio::use_awaitable);
            if (pv) {
                pv.visit(am::overload{[&](am::v3_1_1::puback_packet const& p) {
                                          //
                                          // spdlog::info("MQTT PUBACK recv pid: {0}", p.packet_id());
                                          return;
                                      },
                                      [](auto const&) {}});
            } else {
                auto error_msg = fmt::format("MQTT recv error: {0}", pv.get<am::system_error>().what());
                this->logger()->error(error_msg);
        this->unlock();
                throw IOException(error_msg);
            }
        } catch (...) {
        this->unlock();
            throw;
        }
        this->unlock();
        co_return;
    }

    Awaitable<void> async_publish_string(const std::string_view topic, const std::string_view payload,
                                         async_mqtt::qos qos) override {
        auto topic_buffer = am::allocate_buffer(topic);
        auto payload_buffer = am::allocate_buffer(payload);
        co_await this->async_publish(topic_buffer, payload_buffer, qos);
        co_return;
    }

  private:

    Awaitable<void> async_lock() {
        co_await _lock->async_send(deferred);
        co_return;
    }

    void unlock() {
        _lock->try_receive([](auto...) {});
    }

    Awaitable<void> async_connect() {
        BOOST_ASSERT(_lock);
        this->logger()->info("开始连接 MQTT：{}:{}", this->host(), this->port());

        auto exe = co_await this_coro::executor;

        {
            auto amep = new Endpoint{am::protocol_version::v3_1_1, exe};
            _endpoint = std::move(std::unique_ptr<Endpoint>(amep));
        }

        // asio::ip::tcp::socket resolve_sock{exe};
        asio::ip::tcp::resolver resolver(exe);

        // Resolve hostname
        this->logger()->debug("MqttBrokerNode > 解析地址");

        auto eps = co_await resolver.async_resolve(this->host(), std::to_string(this->port()), asio::use_awaitable);

        // Layer
        // am::stream -> TCP

        // Underlying TCP connect
        this->logger()->debug("MqttBrokerNode > socket 开始连接");
        co_await asio::async_connect(_endpoint->next_layer(), eps, asio::use_awaitable);

        // Send MQTT CONNECT
        if (auto se = co_await _endpoint->send(
                am::v3_1_1::connect_packet{
                    true,   // clean_session
                    0x1234, // keep_alive
                    am::allocate_buffer("cid1"),
                    am::nullopt, // will
                    am::nullopt, // username set like am::allocate_buffer("user1"),
                    am::nullopt  // password set like am::allocate_buffer("pass1")
                },
                asio::use_awaitable)) {
            co_return;
        }

        // Recv MQTT CONNACK
        if (am::packet_variant pv = co_await _endpoint->recv(asio::use_awaitable)) {
            auto cb = am::overload{[&](am::v3_1_1::connack_packet const& p) {
                                       // std::cout << "MQTT CONNACK recv" << " sp:" << p.session_present() <<
                                       // std::endl; spdlog::info("MqttClient > 收到连接相应");
                                   },
                                   [](auto const&) {}};
            pv.visit(cb);
        } else {
            this->logger()->error("MqttClient > CONNACK 收到错误：{0}", pv.get<am::system_error>().what());
            co_return;
        }

        this->logger()->info("MQTT 已连接：{0}:{1}", this->host(), this->port());
    }

    /// @brief 关闭连接
    /// @return
    Awaitable<void> async_close() noexcept {
        BOOST_ASSERT(_lock);
        co_await _endpoint->close(asio::use_awaitable);
        this->logger()->info("MQTT 连接已断开，主机：{0}:{1}", this->host(), this->port());
    }
     

  private:
    std::unique_ptr<Endpoint> _endpoint;

    std::unique_ptr<channel<void()>> _lock; //{socket_.get_executor(), 1};
    //std::unique_ptr<async::AsyncLock<boost::asio::any_io_executor>> _lock;
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<StandaloneNodeProvider<MqttBrokerNode, "mqtt-broker", NodeKind::STANDALONE>>(
        "edgelink::plugins::mqtt::MqttBrokerNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};


}; // namespace edgelink::plugins::mqtt