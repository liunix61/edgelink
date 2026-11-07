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

    // 修复：插件目录不存在时原 directory_iterator 抛异常导致启动崩溃——改为跳过（无插件也能启动）
    if (!fs::exists(plugins_path) || !fs::is_directory(plugins_path)) {
        _logger->warn("插件目录不存在，跳过插件加载：{0}", plugins_path.string());
        return;
    }

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
    auto it = _flow_node_providers.find(type_name);
    if (it == _flow_node_providers.end()) {
        // 修复：原 _flow_node_providers.at() 找不到时抛 out_of_range（无类型信息）——改抛友好异常
        auto error_msg = fmt::format("找不到流程节点类型提供器：type={}", type_name);
        _logger->error(error_msg);
        throw InvalidDataException(error_msg);
    }
    return it->second;
}

const std::unique_ptr<IStandaloneNodeProvider>&
Registry::get_standalone_node_provider(const std::string_view& type_name) const {
    auto it = _standalone_node_providers.find(type_name);
    if (it == _standalone_node_providers.end()) {
        // 修复：原 .at() 抛 out_of_range——改抛友好异常
        auto error_msg = fmt::format("找不到独立节点类型提供器：type={}", type_name);
        _logger->error(error_msg);
        throw InvalidDataException(error_msg);
    }
    return it->second;
}

void Registry::register_node_provider(const rttr::type& provider_type) {

    // 修复：RTTR 跨 DSO 的继承元数据不共享——get_value<IStandaloneNodeProvider*> 对 flow 节点也误判。
    // 改用 INodeProvider*（公共基类）创建 + descriptor()->kind() 精确判断 STANDALONE vs FLOW。
    auto provider_obj = provider_type.create();
    if (!provider_obj.is_valid()) {
        _logger->error("无法创建节点提供器: '{}'", std::string(provider_type.get_name()));
        return;
    }

    // 先尝试 INodeProvider*（公共基类，RTTR 转换对同 TU 可靠）
    auto* provider_base = provider_obj.get_value<INodeProvider*>();
    if (provider_base) {
        auto desc = provider_base->descriptor();
        if (desc && desc->kind() == NodeKind::STANDALONE) {
            auto provider = provider_obj.get_value<IStandaloneNodeProvider*>();
            _logger->info("注册独立节点提供器: '{}'", desc->type_name());
            _standalone_node_providers.emplace(desc->type_name(), std::move(provider));
        } else if (desc) {
            auto provider = provider_obj.get_value<IFlowNodeProvider*>();
            _logger->info("注册流程节点提供器: '{}'", desc->type_name());
            _flow_node_providers.emplace(desc->type_name(), std::move(provider));
        } else {
            _logger->error("节点提供器无 descriptor: '{}'", std::string(provider_type.get_name()));
        }
        return;
    }

    // 回退：get_value<INodeProvider*> 失败（跨 DSO）——用类型名判断（含 Standalone/Broker）
    auto provider_name = std::string(provider_type.get_name());
    auto is_standalone_name =
        provider_name.find("Standalone") != std::string::npos || provider_name.find("Broker") != std::string::npos;
    if (is_standalone_name) {
        auto* raw = provider_obj.get_value<void*>();
        if (raw) {
            auto* standalone_ptr = static_cast<IStandaloneNodeProvider*>(raw);
            auto desc = standalone_ptr->descriptor();
            _logger->info("注册独立节点提供器: '{}'", desc->type_name());
            _standalone_node_providers.emplace(desc->type_name(), std::move(standalone_ptr));
            return;
        }
    }

    // 最后尝试 flow 转换
    auto provider = provider_obj.get_value<IFlowNodeProvider*>();
    if (provider) {
        auto desc = provider->descriptor();
        _logger->info("注册流程节点提供器: '{}'", desc->type_name());
        _flow_node_providers.emplace(desc->type_name(), std::move(provider));
    } else {
        _logger->error("未知的节点提供器: '{}'", std::string(provider_type.get_name()));
    }
}

}; // namespace edgelink