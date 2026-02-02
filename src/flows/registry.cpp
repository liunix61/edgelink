#include "edgelink/edgelink.hpp"
#include "./registry.hpp"

namespace edgelink {

bool is_valid_provider_type(const rttr::type& type) {
    return (type.is_derived_from<IFlowNodeProvider>() || type.is_derived_from<IStandaloneNodeProvider>()) &&
           !type.is_pointer() && type.is_class() && type.get_name() != "edgelink::IStandaloneNodeProvider" &&
           type.get_name() != "edgelink::IFlowNodeProvider";
}

Registry::Registry(const EdgeLinkSettings& el_config) : _logger(spdlog::default_logger()->clone("Flow")), _libs() {

    // auto node_provider_type = rttr::type::get<INodeProvider>();

    // 注册内置节点
    {
        _logger->info("开始注册内置流程节点...");
        // 注册节点提供器
        for (auto& type : rttr::type::get_types()) {
            if (is_valid_provider_type(type)) {
                this->register_node_provider(type);
            }
        }
    }

    namespace fs = std::filesystem;

    auto plugins_path = fs::path(el_config.executable_location / "plugins");
    _logger->info("开始注册插件提供的流程节点，插件目录：{0}", plugins_path.string());

    for (fs::directory_entry const& entry : fs::directory_iterator(plugins_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto lib_path = entry.path().string();
        _logger->info("找到插件：'{0}'", lib_path);
        auto lib = std::make_unique<rttr::library>(lib_path);
        auto is_loaded = lib->load();
        if (!is_loaded) {
            auto error_msg =
                fmt::format("无法加载插件 '{0}'： {1}", lib_path, std::string(lib->get_error_string()));
            throw std::runtime_error(error_msg);
        }

        for (auto type : lib->get_types()) {
            if (is_valid_provider_type(type)) {
                _logger->debug("发现插件节点类型：{0}", std::string(type.get_name()));
                this->register_node_provider(type);
            }
        }
        // 把插件也注册进去
        _libs.emplace_back(std::move(lib));
    }
}

Registry::~Registry() {
    for (auto const& lib : _libs) {
        _logger->info("开始卸载插件动态库：{}", std::string(lib->get_file_name()));
        lib->unload();
    }
}

const std::unique_ptr<IFlowNodeProvider>& Registry::get_flow_node_provider(const std::string_view& type_name) const {
    if (!_flow_node_providers.contains(type_name)) {
        _logger->error("找不到流程节点类型提供器：type={}", type_name);
    }
    return _flow_node_providers.at(type_name);
}

const std::unique_ptr<IStandaloneNodeProvider>&
Registry::get_standalone_node_provider(const std::string_view& type_name) const {
    if (!_standalone_node_providers.contains(type_name)) {
        _logger->error("找不到独立节点类型提供器：type={}", type_name);
    }
    return _standalone_node_providers.at(type_name);
}

void Registry::register_node_provider(const rttr::type& provider_type) {

    auto flow_node_provider_type = rttr::type::get<IFlowNodeProvider>();
    auto standalone_node_provider_type = rttr::type::get<IStandaloneNodeProvider>();

    if (provider_type.is_derived_from(flow_node_provider_type)) {
        auto provider = provider_type.create().get_value<IFlowNodeProvider*>();
        auto desc = provider->descriptor();
        _logger->info("注册流程节点提供器: '{}'", desc->type_name());
        _flow_node_providers.emplace(desc->type_name(), std::move(provider));
    } else if (provider_type.is_derived_from(standalone_node_provider_type)) {
        auto provider = provider_type.create().get_value<IStandaloneNodeProvider*>();
        auto desc = provider->descriptor();
        _logger->info("注册流程节点提供器: '{}'", desc->type_name());
        _standalone_node_providers.emplace(desc->type_name(), std::move(provider));
    } else {
        _logger->error("未知的节点提供器: '{}'", std::string(provider_type.get_name()));
    }
}

}; // namespace edgelink