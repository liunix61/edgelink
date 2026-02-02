#pragma once

namespace edgelink {

/// @brief 插件注册管理器
struct EDGELINK_EXPORT IRegistry {
    //
    virtual const std::unique_ptr<IFlowNodeProvider>& get_flow_node_provider(const std::string_view& name) const = 0;

    virtual const std::unique_ptr<IStandaloneNodeProvider>&
    get_standalone_node_provider(const std::string_view& name) const = 0;

};

}; // namespace edgelink
