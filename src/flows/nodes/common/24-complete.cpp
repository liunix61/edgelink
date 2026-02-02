#include "edgelink/edgelink.hpp"

using namespace edgelink;

class CompleteNode : public ScopedSourceNode {
  public:
    CompleteNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : ScopedSourceNode(id, desc, flow, config) {}

    virtual ~CompleteNode() {
        // TODO 断开连接
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> on_async_run() override { co_return; }
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<CompleteNode, "complete", NodeKind::SOURCE>>(
        "edgelink::CompleteNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};