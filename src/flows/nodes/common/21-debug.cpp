#include "edgelink/edgelink.hpp"

using namespace edgelink;

/*
    {
        "id": "adde85bf75a42c9c",
        "type": "debug",
        "z": "73e0fcd142fc5256",
        "name": "debug 1",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "true",
        "targetType": "full",
        "statusVal": "",
        "statusType": "auto",
        "x": 920,
        "y": 380,
        "wires": []
    }
*/

class DebugNode : public SinkNode {
  public:
    DebugNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : SinkNode(id, desc, flow, config) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        //
        fmt::print("node {0}\n{0}\n", this->name(), msg->to_string());
        co_return;
    }

    /*
  private:
    const bool _active;
    const bool _tosidebar;
    const bool _console;
    const bool _tostatus;
    const bool _complete;
    const std::string target_type;
    const std::string _status_val;
    const std::string _status_type;
*/
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<DebugNode, "debug", NodeKind::SINK>>("edgelink::DebugNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};