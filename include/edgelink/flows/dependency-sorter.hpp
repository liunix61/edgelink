#pragma once

namespace edgelink {

template <typename T> class DependencySorter {
  public:
    void add_edge(const T& source, const T& destination) { adjacency_list[source].insert(destination); }
    void clear() { this->adjacency_list.clear(); }

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
        while (!zero_in_degree_queue.empty()) {
            T current = zero_in_degree_queue.front();
            zero_in_degree_queue.pop();

            // 反向添加到结果向量
            topological_order.insert(topological_order.begin(), current);

            for (const T& neighbor : adjacency_list[current]) {
                in_degree[neighbor]--;
                if (in_degree[neighbor] == 0) {
                    zero_in_degree_queue.push(neighbor);
                }
            }
        }

        return topological_order;
    }

  private:
    std::unordered_map<T, std::unordered_set<T>> adjacency_list;
};

}; // namespace edgelink