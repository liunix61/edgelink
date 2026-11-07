#include "edgelink/edgelink.hpp"

using namespace edgelink;

class BlackholeNode : public SinkNode {
  public:
    BlackholeNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc,
                  IFlow* flow)
        : SinkNode(id, desc, flow, config) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        // 修复：Msg 的消息 ID 存在 "_msgid" 字段（非 "id"）——原来 at("id") 找不到键会崩溃
        MsgID msg_id = msg->data().at("_msgid").get<MsgID>();
        spdlog::info("BlackholeNode > 吃掉了消息：[msg.id={0}]", msg_id);
        co_return;
    }
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<BlackholeNode, "blackhole", NodeKind::SINK>>(
        "edgelink::BlackholeNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
