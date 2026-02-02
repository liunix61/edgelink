#include <edgelink/plugin.hpp>

using namespace edgelink;

namespace edgelink::plugins::modbus {

class ModbusLogNode : public SinkNode {
  public:
    ModbusLogNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                  IFlow* flow)
        : SinkNode(id, desc, flow, config) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        //
        spdlog::info("ModBusLogNode > 收到了消息：\n{0}", msg->to_string());
        co_return;
    }
};

RTTR_PLUGIN_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<ModbusLogNode, "modbus.log", NodeKind::SINK>>(
        "edgelink::plugins::modbus::ModbusLogNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
}; // namespace edgelink::plugins::modbus