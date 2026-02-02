#pragma once

namespace edgelink {

struct IStandaloneNode;
struct IFlow;
struct IFlowNode;

}; // namespace edgelink

namespace edgelink::flows {

class FlowFactory : public edgelink::IFlowFactory, private Noncopyable {
  public:
    FlowFactory(const IRegistry& registry);

    std::vector<std::unique_ptr<IFlow>> create_flows(const JsonArray& flows_config, IEngine* engine) const override;

    std::vector<std::unique_ptr<edgelink::IStandaloneNode>> create_global_nodes(const JsonArray& flows_config,
                                                                                IEngine* engine) const override;

  private:
    std::unique_ptr<edgelink::IFlow> create_flow(const JsonArray& flows_config,
                                                 const JsonObject& flow_node, IEngine* engine) const;

  private:
    std::shared_ptr<spdlog::logger> _logger;
    const IRegistry& _registry;

};

}; // namespace edgelink::flows