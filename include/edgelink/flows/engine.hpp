#pragma once

namespace edgelink {

struct IFlow;
struct IStandaloneNode;

/// @brief 数据处理引擎接口
struct EDGELINK_EXPORT IEngine {
    virtual const EdgeLinkSettings& settings() const = 0;
    virtual Awaitable<void> async_start() = 0;
    virtual Awaitable<void> async_stop() = 0;
    virtual IFlow* get_flow(const std::string_view flow_id) const = 0;
    virtual IStandaloneNode* get_global_node(const std::string_view node_id) const = 0;
    virtual bool is_disabled() const = 0;

};

}; // namespace edgelink