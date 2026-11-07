#include "edgelink/edgelink.hpp"
#include "edgelink/flows/dependency-sorter.hpp"
#include "flow.hpp"
#include "flow-factory.hpp"

using namespace edgelink;

namespace edgelink::flows {

FlowFactory::FlowFactory(const IRegistry& registry)
    : _logger(spdlog::default_logger()->clone("Flow")), _registry(registry) {
    //
}

std::vector<std::unique_ptr<IFlow>> FlowFactory::create_flows(const JsonArray& flows_config,
                                                              IEngine* engine) const {
    // auto node_provider_type = rttr::type::get<IFlowNodeProvider>();

    // auto dataflow_elements = flows_config["dataflow"];
    std::vector<std::unique_ptr<IFlow>> flows;
    for (const auto& json_node_value : flows_config) {
        const auto& json_node = json_node_value.is_object() ? json_node_value : JsonObject::object();
        const std::string type(json_node.at("type").get<std::string>());
        if (type == "tab" || type == "flow") {
            try {
                auto flow = this->create_flow(flows_config, json_node, engine);
                flows.emplace_back(std::move(flow));
            } catch (std::exception& ex) {
                _logger->error("创建流时发生错误：{0}", ex.what());
                throw;
            }
        }
    }
    return flows;
}

std::vector<std::unique_ptr<IStandaloneNode>> FlowFactory::create_global_nodes(const JsonArray& flows_config,
                                                                               IEngine* engine) const {
    // 创建全局节点
    std::vector<std::unique_ptr<IStandaloneNode>> global_nodes;
    for (const auto& json_node_value : flows_config) {
        // 修复：`json_node_value.is_object() ? json_node_value : JsonObject::object()` ternary 临时 → json_node 悬垂。
        //       改为 if-early-continue（非 object 跳过），保证 json_node 引用稳定的 flows_config 元素。
        if (!json_node_value.is_object()) {
            continue;
        }
        const JsonObject& json_node = json_node_value;
        const std::string elem_type = json_node.at("type").get<std::string>();
        const std::string elem_id = json_node.at("id").get<std::string>();
        if (elem_type != "tab" && elem_type != "flow" && !json_node.contains("z")) {
            auto const& provider_iter = _registry.get_standalone_node_provider(elem_type);
            _logger->info("开始创建独立节点：[type='{0}', id='{1}']", elem_type, elem_id);
            try {
                auto node = provider_iter->create(elem_id, json_node, engine);
                global_nodes.emplace_back(std::move(node));
            } catch (std::exception& ex) {
                _logger->error("开始创建独立节点：[type='{0}', id='{1}'] 发生错误：{2}", elem_type, elem_id, ex.what());
                throw;
            }
        }
    }

    return global_nodes;
}

std::unique_ptr<IFlow> FlowFactory::create_flow(const JsonArray& flows_config,
                                                const JsonObject& flow_node, IEngine* engine) const {

    // 创建边连接
    // 修复：DependencySorter<std::string_view> 存 string_view 引用局部 std::string（已析构）→ 悬垂乱码。
    //       改用 DependencySorter<std::string>（值语义，sorter 持有 string）。
    DependencySorter<std::string> sorter;

    auto flow_node_id = flow_node.at("id").get<std::string>();
    // 创建一个空的流
    auto flow = std::make_unique<Flow>(flow_node, engine);

    // 提取属于指定流节点的下级节点
    std::unordered_map<std::string, const JsonObject*> json_nodes;
    for (const auto& elem_value : flows_config) {
        // 修复：`elem_value.is_object() ? elem_value : JsonObject::object()` ternary 结果类型是值（JsonObject），
        //       `const auto& elem` 绑定到 ternary 临时 → 临时析构后 elem 悬垂 → :86 存悬垂指针 → :129 崩溃。
        //       改为显式局部变量持有（若非 object 用空对象），保证 elem 生命周期覆盖整个循环体。
        if (!elem_value.is_object()) {
            continue;
        }
        const JsonObject& elem = elem_value;
        const std::string elem_type = elem.at("type").get<std::string>();

        // 跳过全局节点和注释
        if (!elem.contains("z") || elem_type == "comment") {
            continue;
        }
        const std::string z = elem.at("z").get<std::string>();
        if (z == flow_node_id) {
            const std::string node_id = elem.at("id").get<std::string>();
            json_nodes[node_id] = &elem;
            // 修复：wires 属性可缺失（纯输出节点如 status/complete 无 wires）——防御性检查，避免 at() 抛异常
            if (elem.contains("wires")) {
                for (const auto& port : elem.at("wires")) {
                    for (const auto& endpoint : port) {
                        // 修复：std::string_view 悬垂引用（get<std::string>() 返回临时）——改用 std::string 持有
                        const std::string from = node_id;
                        const std::string to = endpoint.get<std::string>();
                        // 修复：wires 引用不存在的节点（悬空 wire）——跳过加边，避免拓扑排序/创建时崩溃
                        if (json_nodes.find(to) != json_nodes.end() ||
                            std::any_of(flows_config.begin(), flows_config.end(), [&to](const auto& nv) {
                                if (!nv.is_object()) return false;
                                const auto& obj = nv;
                                return obj.contains("id") && obj.at("id") == to;
                            })) {
                            sorter.add_edge(from, to);
                        }
                    }
                }
            }
            // 如果有 scope 属性也计算在内
            if (elem.contains("scope")) {
                for (const auto& scoped_node_id : elem.at("scope")) {
                    // 修复：string_view 悬垂（get<std::string>() 临时）——改 std::string
                    const std::string from = node_id;
                    const std::string to = scoped_node_id.get<std::string>();
                    sorter.add_edge(from, to);
                }
            }
        }
    }

    // 先把 json 节点提取出来

    auto sorted_ids = sorter.sort();

    std::unordered_map<std::string, IFlowNode*> node_map;

    for (size_t i = 0; i < sorted_ids.size(); i++) {
        const std::string elem_id = sorted_ids[i];
        // 修复：sorted_ids 可能含不属于本流程的节点（悬空 wire/scope 目标）——跳过，避免 .at() 崩溃
        auto it = json_nodes.find(std::string(elem_id));
        if (it == json_nodes.end()) {
            _logger->warn("创建流程节点时跳过不属于本流程的节点 id：{}", elem_id);
            continue;
        }
        const JsonObject& elem = *it->second;
        // 修复：`const auto& elem_type = elem.at("type").get<std::string>();` auto& 绑定临时 string → 悬垂
        //       （get<std::string>() 返回临时，elem_type 是悬垂引用 → get_flow_node_provider 查错乱）
        //       改用 std::string 值持有。
        const std::string elem_type = elem.at("type").get<std::string>();

        _logger->info("创建流程节点：[type='{0}', json_id='{1}']", elem_type, elem_id);
        auto const& provider_iter = _registry.get_flow_node_provider(elem_type);
        try {
            auto node = provider_iter->create(elem_id, elem, flow.get());
            node_map[std::string(elem_id)] = node.get();
            flow->emplace_node(std::move(node));
        } catch (std::exception& ex) {
            _logger->error("创建流程节点：[type='{0}', id='{1}'] 发生错误：{2}", elem_type, elem_id, ex.what());
            throw;
        }
    }
    return std::move(flow);
}

}; // namespace edgelink::flows