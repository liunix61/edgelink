#pragma once

namespace edgelink {

struct IFlowNode;

using MsgRoutingPath = boost::container::static_vector<const IFlowNode*, 32>;

struct EdgeLinkSettings;

class Engine : public IEngine, private std::enable_shared_from_this<Engine>, private Noncopyable {
  public:
    Engine(const EdgeLinkSettings& el_config, const IFlowFactory& flow_factory);
    virtual ~Engine();

    bool is_disabled() const override { return _disabled; }

    const EdgeLinkSettings& settings() const override { return _settings; }

    Awaitable<void> async_start() override;
    Awaitable<void> async_stop() override;

    inline IFlow* get_flow(const std::string_view flow_id) const override {
        for (auto& flow : _flows) {
            if (flow->id() == flow_id) {
                return flow.get();
            }
        }
        return nullptr;
    }

    inline IStandaloneNode* get_global_node(const std::string_view node_id) const override {
        for (auto& node : _global_nodes) {
            if (node->id() == node_id) {
                return node.get();
            }
        }
        return nullptr;
    }

  private:
    std::shared_ptr<spdlog::logger> _logger;
    const EdgeLinkSettings& _settings;
    const IFlowFactory& _flow_factory;
    const std::string _flows_json_path;
    std::vector<std::unique_ptr<IStandaloneNode>> _global_nodes;

    std::unique_ptr<std::stop_source> _stop_source;
    bool _disabled;
    std::vector<std::unique_ptr<IFlow>> _flows;

};

}; // namespace edgelink