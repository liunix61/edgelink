#include "edgelink/edgelink.hpp"

using namespace edgelink;

class StatusNode : public ScopedSourceNode {
  public:
    StatusNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : ScopedSourceNode(id, desc, flow, config) {
        //
    }

    virtual ~StatusNode() {
        // TODO 断开连接
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> on_async_run() override { co_return; }
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<StatusNode, "status", NodeKind::SOURCE>>("edgelink::StatusNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};