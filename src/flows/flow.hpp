#pragma once

#include "edgelink/utils.hpp"

namespace edgelink {
class FlowNode;
struct IRegistry;
struct IEngine;
}; // namespace edgelink

namespace edgelink::flows {

class Flow : public IFlow, private Noncopyable {

  public:
    Flow(const JsonObject& json_config, IEngine* engine);
    virtual ~Flow();

    FlowOnSendEvent& on_send_event() override { return _on_send_event; }
    FlowPreRouteEvent& on_pre_route_event() override { return _on_pre_route_event; }
    FlowPreDeliverEvent& on_pre_deliver_event() override { return _on_pre_deliver_event; }
    FlowPostDeliverEvent& on_post_deliver_event() override { return _on_post_deliver_event; }

    const std::string_view id() const override { return _id; }
    const std::string_view label() const override { return _label; }
    bool is_disabled() const override { return _disabled; }
    IEngine* engine() const override { return _engine; }

    Awaitable<void> async_start() override;
    Awaitable<void> async_stop() override;

    Awaitable<void> async_send_many(std::vector<std::unique_ptr<Envelope>>&& envelopes) override;

    IFlowNode* get_node(const std::string_view id) const override;

    inline void emplace_node(std::unique_ptr<IFlowNode>&& node) { _nodes.emplace_back(std::move(node)); }

  private:
    Awaitable<void> async_send_one_internal(std::unique_ptr<Envelope> envelope);

  private:
    std::shared_ptr<spdlog::logger> _logger;
    const std::string _id;
    const std::string _label;
    const bool _disabled;
    IEngine* const _engine;
    std::vector<std::unique_ptr<IFlowNode>> _nodes;

    std::atomic<uint64_t> _msg_id_counter; // 初始化计数器为0

    std::unique_ptr<std::stop_source> _stop_source;

  private:
    FlowOnSendEvent _on_send_event;
    FlowPreRouteEvent _on_pre_route_event;
    FlowPreDeliverEvent _on_pre_deliver_event;
    FlowPostDeliverEvent _on_post_deliver_event;
};

}; // namespace edgelink::flows
