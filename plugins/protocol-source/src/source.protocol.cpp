#include <edgelink/plugin.hpp>

#include "shm_event_queue.h"

using namespace edgelink;

namespace edgelink::plugins::protocolsource {

/* config:
 * {
 *   "type": "protocol.source",
 *   "shm": "/webhmi_events",        // 共享内存名（可选，默认 /webhmi_events）
 *   "protocol": "modbus",           // 协议过滤（可选：canopen/ethercat/modbus/any）
 *   "topic": "protocol/modbus",     // 消息 topic（可选）
 * }
 */
class ProtocolSourceNode : public SourceNode {
  public:
    ProtocolSourceNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                       IFlow* flow)
        : SourceNode(id, desc, flow, config) {
        if (auto it = config.find("shm"); it != config.end()) {
            _shm_name = it->get<std::string>();
        }
        if (auto it = config.find("protocol"); it != config.end()) {
            _protocol = it->get<std::string>();
        }
        if (auto it = config.find("topic"); it != config.end()) {
            _topic = it->get<std::string>();
        }
    }

    ~ProtocolSourceNode() = default;

  protected:
    Awaitable<void> on_async_run() override {
        // 打开共享内存事件队列（只读端）
        _queue = shm_event_queue_open(0, 0 /* 仅打开 */);
        if (_queue == nullptr) {
            this->logger()->error("protocol.source 打开共享内存事件队列失败：{0}", _shm_name);
            co_return;
        }

        while (!_stop_requested.load()) {
            shm_event_t ev;
            int r = shm_event_queue_pop(_queue, &ev);
            if (r != 0) {
                // 空：让出调度（协程友好）
                auto executor = co_await this_coro::executor();
                SteadyTimer timer(executor, std::chrono::milliseconds(5));
                co_await timer.async_wait();
                continue;
            }

            // 协议过滤
            if (!_protocol.empty() && _protocol != "any") {
                if (protocol_to_str(ev.protocol) != _protocol) {
                    continue;
                }
            }

            // 构建 Msg
            auto msg = std::make_shared<Msg>(this);
            JsonObject& data = msg->data();
            data["protocol"] = protocol_to_str(ev.protocol);
            data["op"] = op_to_str(ev.op);
            data["slave"] = ev.slave_id;
            data["index"] = ev.index;
            data["sub"] = ev.sub;
            data["value"] = ev.value;
            data["ts_ns"] = (double)ev.ts_ns;
            data["error"] = ev.error;
            if (!_topic.empty()) {
                data["topic"] = _topic;
            }

            co_await this->async_send_to_one_port(std::move(msg));
        }
        /* 退出前关闭共享内存 */
        if (_queue != nullptr) {
            shm_event_queue_close(_queue);
            _queue = nullptr;
        }
        co_return;
    }

    Awaitable<void> async_stop() override {
        _stop_requested.store(true);
        co_return;
    }

  private:
    static std::string protocol_to_str(uint32_t p) {
        switch (p) {
        case 1: return "canopen";
        case 2: return "ethercat";
        case 3: return "modbus";
        default: return "unknown";
        }
    }
    static std::string op_to_str(uint32_t op) {
        switch (op) {
        case 1: return "reg_update";
        case 2: return "sdo_response";
        case 3: return "coe_response";
        case 4: return "nmt_status";
        default: return "unknown";
        }
    }

    std::string _shm_name{SHM_EQ_NAME};
    std::string _protocol; /* 空=不过滤 */
    std::string _topic;
    shm_eq_header_t *_queue{nullptr};
    std::atomic<bool> _stop_requested{false};
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<ProtocolSourceNode, "protocol.source", NodeKind::SOURCE>>(
        "edgelink::plugins::protocolsource::ProtocolSourceNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
}; // namespace edgelink::plugins::protocolsource
