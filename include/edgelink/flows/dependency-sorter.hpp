#pragma once

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace edgelink {

/// @brief 依赖排序器（拓扑排序，Kahn 算法）
/// @note EdgeLink 节点创建依赖：wires 目标节点必须先于源节点创建（setup_output_ports 需查目标）。
///       故 sort() 返回"反向拓扑序"（目标在前，源在后），保证节点构造时输出端口目标已存在。
template <typename T> class DependencySorter {
  public:
    void add_edge(const T& source, const T& destination) { adjacency_list[source].insert(destination); }
    void clear() { this->adjacency_list.clear(); }

    /// @brief 拓扑排序（反向序：依赖目标在前）。
    ///        有向环时环节点 in_degree 永不为 0，被排除在拓扑序外（环是配置错误，由 has_cycle() 检测）。
    ///        返回仅含无环依赖的节点，保证流程加载不因环节点中断。
    std::vector<T> sort() {
        std::unordered_map<T, int> in_degree;
        for (const auto& pair : adjacency_list) {
            in_degree[pair.first] = 0;
        }

        for (const auto& pair : adjacency_list) {
            for (const T& neighbor : pair.second) {
                in_degree[neighbor]++;
            }
        }

        std::queue<T> zero_in_degree_queue;
        for (const auto& pair : in_degree) {
            if (pair.second == 0) {
                zero_in_degree_queue.push(pair.first);
            }
        }

        std::vector<T> topological_order;
        std::unordered_set<T> processed;
        while (!zero_in_degree_queue.empty()) {
            T current = zero_in_degree_queue.front();
            zero_in_degree_queue.pop();

            if (processed.count(current)) {
                continue;
            }
            processed.insert(current);

            // 反向添加到结果向量（目标在前，源在后——满足 setup_output_ports 查询顺序）
            topological_order.insert(topological_order.begin(), current);

            for (const T& neighbor : adjacency_list[current]) {
                in_degree[neighbor]--;
                if (in_degree[neighbor] == 0) {
                    zero_in_degree_queue.push(neighbor);
                }
            }
        }

        // 有向环时环节点未处理（in_degree 永不为 0）——不追加（环节点是配置错误）
        // 由 has_cycle() 检测后由调用方（flow-factory）记录错误并降级处理
        return topological_order;
    }

    /// @brief 是否存在有向环（环节点未被拓扑排序处理）
    bool has_cycle() const {
        std::unordered_map<T, int> in_degree;
        for (const auto& pair : adjacency_list) {
            in_degree[pair.first] = 0;
        }
        for (const auto& pair : adjacency_list) {
            for (const T& neighbor : pair.second) {
                in_degree[neighbor]++;
            }
        }
        std::queue<T> zero_in_degree_queue;
        for (const auto& pair : in_degree) {
            if (pair.second == 0) {
                zero_in_degree_queue.push(pair.first);
            }
        }
        size_t visited = 0;
        while (!zero_in_degree_queue.empty()) {
            T current = zero_in_degree_queue.front();
            zero_in_degree_queue.pop();
            visited++;
            for (const T& neighbor : adjacency_list[current]) {
                in_degree[neighbor]--;
                if (in_degree[neighbor] == 0) {
                    zero_in_degree_queue.push(neighbor);
                }
            }
        }
        return visited < adjacency_list.size();
    }

  private:
    std::unordered_map<T, std::unordered_set<T>> adjacency_list;
};

}; // namespace edgelink
