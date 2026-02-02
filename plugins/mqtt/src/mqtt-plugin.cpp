#include <edgelink/plugin.hpp>

namespace edgelink::plugins::mqtt {

class MqttPlugin : public edgelink::IPlugin {
  public:
    MqttPlugin() : _name("MQTT Plugin") {}

    std::string_view name() const override { return _name; }

    const std::unordered_map<std::string, std::unique_ptr<INodeDescriptor>>& node_descriptors() const override {
        return _node_descriptors;
    }

  private:
    const std::string _name;
    const std::unordered_map<std::string, std::unique_ptr<INodeDescriptor>> _node_descriptors;
};

EDGELINK_PLUGIN_DEFINE(MqttPlugin);

}; // namespace edgelink::plugins::mqtt

// BOOST_DLL_ALIAS(edgelink::plugins::mqtt::MqttPlugin, plugin)
