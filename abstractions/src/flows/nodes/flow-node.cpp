
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
            auto out_node = flow->get_node(endpoint.as_string());
            output_wires.push_back(out_node);
        }
        auto port = OutputPort(std::move(output_wires));
        ports.emplace_back(std::move(port));
    }
    return ports;
}

}; // namespace edgelink