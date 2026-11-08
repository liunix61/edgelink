#include <edgelink/plugin.hpp>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/transport/ws/websocket.h>

#include <cstdio>
#include <mutex>

using namespace edgelink;

namespace edgelink::plugins::protocolsource {

/*
 * C3 展示节点（SinkNode）：收 workflow 消息 → nng WS pub 广播（HMI 订阅）
 *
 * config:
 * {
 *   "type": "protocol.display",
 *   "name": "HMI 展示",
 *   "ws_url": "ws://0.0.0.0:8081/ws",   // WS 广播地址（可选，默认）
 *   "topic": "hmi"                       // 消息前缀过滤（可选）
 * }
 *
 * receive_async(msg)：msg->data() 序列化 JSON → nng_pub 广播
 * 浏览器/客户端连 ws://<host>:8081/ws 订阅实时数据。
 */
class ProtocolDisplayNode : public SinkNode {
  public:
    ProtocolDisplayNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                        IFlow* flow)
        : SinkNode(id, desc, flow, config) {
        if (auto it = config.find("ws_url"); it != config.end()) {
            _ws_url = it->get<std::string>();
        }
        if (auto it = config.find("topic"); it != config.end()) {
            _topic = it->get<std::string>();
        }
        _open_ws();
    }

    Awaitable<void> async_start() override { co_return; }
    Awaitable<void> async_stop() override {
        if (_sock.id != 0) {
            nng_close(_sock);
            _sock.id = 0;
        }
        co_return;
    }

    Awaitable<void> receive_async(MsgPtr msg) override {
        if (_sock.id == 0) {
            co_return;
        }
        const JsonObject& data = msg->data();
        // 主题过滤
        if (!_topic.empty()) {
            auto it = data.find("topic");
            if (it == data.end() || it->get<std::string>() != _topic) {
                co_return;
            }
        }
        std::string payload = data.dump();
        // nng_pub 广播（nng 发送是原子的，非阻塞到缓冲）
        int rv = nng_send(_sock, (void*)payload.data(), payload.size(), 0);
        if (rv != 0) {
            this->logger()->warn("protocol.display WS 广播失败：{0}", nng_strerror(rv));
        }
        co_return;
    }

  private:
    void _open_ws() {
        nng_ws_register();
        int rv = nng_pub_open(&_sock);
        if (rv != 0) {
            this->logger()->error("protocol.display nng_pub_open 失败：{0}", nng_strerror(rv));
            _sock.id = 0;
            return;
        }
        rv = nng_listen(_sock, _ws_url.c_str(), nullptr, 0);
        if (rv != 0) {
            this->logger()->error("protocol.display WS listen 失败：{0}（{1}）", nng_strerror(rv), _ws_url);
            nng_close(_sock);
            _sock.id = 0;
            return;
        }
        this->logger()->info("protocol.display WS 广播已启动：{0}", _ws_url);
    }

    std::string _ws_url{"ws://0.0.0.0:8081/ws"};
    std::string _topic;
    nng_socket _sock{};
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<ProtocolDisplayNode, "protocol.display", NodeKind::SINK>>(
        "edgelink::plugins::protocolsource::ProtocolDisplayNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
}; // namespace edgelink::plugins::protocolsource
