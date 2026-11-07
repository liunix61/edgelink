
#include "edgelink/edgelink.hpp"

namespace edgelink {

Awaitable<void> FlowNode::receive_async(MsgPtr msg) {
    // 默认就是什么都不干
    co_return;
}

Awaitable<void> FlowNode::async_send_to_one_port(MsgPtr msg) {
    if (this->output_ports().size() != 1) {
        this->logger()->warn("节点 (id={}, name={}) 必须只有一个端口才能调用本方法", this->id(), this->name());
        co_return;
    }

    std::vector<MsgPtr> envelopes;
    envelopes.emplace_back(msg);

    co_await this->async_send_to_many_port(std::move(envelopes));
    co_return;
}

Awaitable<void> FlowNode::async_send_to_many_port(std::vector<MsgPtr>&& msgs) {
    auto&& ports = this->output_ports();
    if (msgs.size() > ports.size()) {
        auto error_msg = "发送的消息超出端口数量";
        this->logger()->error(error_msg);
        throw std::out_of_range(error_msg);
    }
    std::vector<std::unique_ptr<Envelope>> envelopes;
    bool msg_sent = false;
    for (size_t iport = 0; iport < msgs.size(); iport++) {
        auto&& port = &ports.at(iport);
        for (size_t iwire = 0; iwire < port->wires().size(); iwire++) {

            IFlowNode* dest_node = port->wires().at(iwire);

            auto env = std::make_unique<Envelope>(msgs.at(iport), msg_sent, this->id(), this, port, dest_node->id(),
                                                  dest_node);

            envelopes.emplace_back(std::move(env));
            msg_sent = true;
        }
    }
    auto flow = this->flow();
    BOOST_ASSERT(flow != nullptr);
    co_await flow->async_send_many(std::move(envelopes));
    co_return;
}

const std::vector<OutputPort> FlowNode::setup_output_ports(const JsonObject& config, IFlow* flow) {
    auto ports = std::vector<OutputPort>();
    for (const auto& port_config : config.at("wires").as_array()) {
        auto output_wires = std::vector<IFlowNode*>();
        for (const auto& endpoint : port_config.as_array()) {
            // 修复：Node-RED 流中 wires 可能引用不存在的节点 ID（悬空 wire）——原 get_node 抛异常崩溃
            // 改为容忍：跳过悬空 wire 并记录警告，不中断流程加载
            auto endpoint_str = endpoint.as_string();
            try {
                auto out_node = flow->get_node(endpoint_str);
                output_wires.push_back(out_node);
            } catch (const std::exception& ex) {
                spdlog::warn("节点 (id={}) 的 wires 引用不存在的节点（{}），已跳过该连接：{}", config.at("id").as_string(),
                             endpoint_str, ex.what());
            }
        }
        auto port = OutputPort(std::move(output_wires));
        ports.emplace_back(std::move(port));
    }
    return ports;
}

}; // namespace edgelink