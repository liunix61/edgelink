#include <edgelink/plugin.hpp>

#include "shm_command_queue.h"

using namespace edgelink;

namespace edgelink::plugins::protocolsource {

/* config:
 * {
 *   "type": "protocol.command",
 *   "shm": "/webhmi_commands",   // 共享内存名（可选，默认 /webhmi_commands）
 *   "protocol": "modbus",        // 目标协议（可选，默认 modbus）
 * }
 *
 * receive_async(msg)：解析 msg（protocol/slave/index/sub/value/timeout_ms）→ 写 shm 命令队列
 * 示例消息：
 *   { "payload": { "protocol":"modbus", "slave":1, "index":0x100, "value":42 } }
 * 或扁平：
 *   { "protocol":"modbus", "slave":1, "index":0x100, "value":42 }
 */
class ProtocolCommandNode : public SinkNode {
  public:
    ProtocolCommandNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                        IFlow* flow)
        : SinkNode(id, desc, flow, config) {
        if (auto it = config.find("shm"); it != config.end()) {
            _shm_name = it->get<std::string>();
        }
        if (auto it = config.find("protocol"); it != config.end()) {
            _protocol = it->get<std::string>();
        }
        // 打开命令队列（写端）；若不存在则创建（webhmi 未启动时兜底）
        _queue = shm_command_queue_open(0, 1 /* create 兜底 */);
        if (_queue == nullptr) {
            this->logger()->error("protocol.command 打开共享内存命令队列失败：{0}", _shm_name);
        }
    }

    Awaitable<void> async_start() override { co_return; }
    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        if (_queue == nullptr) {
            co_return;
        }
        const JsonObject& data = msg->data();

        // 兼容 payload 对象嵌套
        const JsonObject* src = &data;
        if (auto it = data.find("payload"); it != data.end() && it->is_object()) {
            src = &(*it);
        }

        // 解析命令
        shm_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.protocol = protocol_to_code(*src);
        cmd.op = op_to_code(*src);
        cmd.slave_id = get_u32(*src, "slave", get_u32(*src, "slave_id", 1));
        cmd.index = get_u32(*src, "index", 0);
        cmd.sub = get_u32(*src, "sub", 0);
        cmd.value = get_u32(*src, "value", 0);
        cmd.timeout_ms = get_u32(*src, "timeout_ms", 1000);

        // 协议过滤
        if (!_protocol.empty() && _protocol != "any") {
            if (protocol_to_str(cmd.protocol) != _protocol) {
                co_return;
            }
        }

        int r = shm_command_queue_push(_queue, &cmd);
        if (r != 0) {
            this->logger()->warn("protocol.command 命令队列满，丢弃命令");
        } else {
            this->logger()->debug("protocol.command 下发命令：proto={0} slave={1} idx={2} val={3}",
                                  protocol_to_str(cmd.protocol), cmd.slave_id, cmd.index, cmd.value);
        }
        co_return;
    }

  private:
    static uint32_t get_u32(const JsonObject& obj, const std::string& key, uint32_t def) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        if (it->is_number_integer() || it->is_number_unsigned()) {
            return (uint32_t)it->get<int64_t>();
        }
        if (it->is_string()) {
            try {
                return (uint32_t)std::stoul(it->get<std::string>());
            } catch (...) {
                return def;
            }
        }
        return def;
    }
    static uint32_t protocol_to_code(const JsonObject& obj) {
        auto it = obj.find("protocol");
        if (it != obj.end() && it->is_string()) {
            std::string p = it->get<std::string>();
            if (p == "canopen") return 1;
            if (p == "ethercat") return 2;
            if (p == "modbus") return 3;
        }
        return 3; /* 默认 modbus */
    }
    static uint32_t op_to_code(const JsonObject& obj) {
        auto it = obj.find("op");
        if (it != obj.end() && it->is_string()) {
            std::string o = it->get<std::string>();
            if (o == "reg_update") return 1;
            if (o == "sdo_response") return 2;
            if (o == "coe_response") return 3;
            if (o == "nmt_status") return 4;
        }
        return 1; /* 默认 reg_update */
    }
    static std::string protocol_to_str(uint32_t p) {
        switch (p) {
        case 1: return "canopen";
        case 2: return "ethercat";
        case 3: return "modbus";
        default: return "unknown";
        }
    }

    std::string _shm_name{SHM_CMD_NAME};
    std::string _protocol;
    shm_cmd_header_t *_queue{nullptr};
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<ProtocolCommandNode, "protocol.command", NodeKind::SINK>>(
        "edgelink::plugins::protocolsource::ProtocolCommandNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
}; // namespace edgelink::plugins::protocolsource
