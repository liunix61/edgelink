#include "edgelink/edgelink.hpp"

using namespace edgelink;

class CatchNode : public ScopedSourceNode {
  public:
    CatchNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : ScopedSourceNode(id, desc, flow, config), _uncaught(config.at("uncaught").as_bool()) {}

    virtual ~CatchNode() {
        // TODO 断开连接
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> on_async_run() override { co_return; }

  private:
    const bool _uncaught;
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<CatchNode, "catch", NodeKind::SOURCE>>("edgelink::CatchNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};