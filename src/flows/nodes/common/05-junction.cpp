#include "edgelink/edgelink.hpp"

using namespace edgelink;

class JunctionNode : public FlowNode {
  public:
    JunctionNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        // 直接分发消息
        co_await this->async_send_to_one_port(msg);
    }
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<JunctionNode, "junction", NodeKind::JUNCTION>>(
        "edgelink::JunctionNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};