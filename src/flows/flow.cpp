#include "edgelink/edgelink.hpp"
#include "edgelink/flows/dependency-sorter.hpp"
#include "flow.hpp"

using namespace edgelink;
using namespace boost;
namespace this_coro = boost::asio::this_coro;

// using CloneMsgStaticVector = boost::container::static_vector<std::shared_ptr<edgelink::Msg>, 32>;

namespace edgelink::flows {

Flow::Flow(const JsonObject& json_config, IEngine* engine)
    : _logger(spdlog::default_logger()->clone("Flow")), _id(json_config.at("id").as_string()),
      _label(json_config.at("label").as_string()), _disabled(edgelink::value_or(json_config, "disabled", true)),
      _engine(engine), _nodes() {
    BOOST_ASSERT(engine != nullptr);
}

Flow::~Flow() {
    //
}

Awaitable<void> Flow::async_start() {
    //
    auto executor = co_await boost::asio::this_coro::executor;
    _stop_source = std::make_unique<std::stop_source>();

    for (auto const& node : _nodes) {
        _logger->debug("正在启动流程节点：[id={0}, type={1}]", node->id(), node->type());
        boost::asio::co_spawn(executor, node->async_start(), boost::asio::detached);
        _logger->debug("流程节点已启动");
    }
}

Awaitable<void> Flow::async_stop() {
    // 给出线程池停止信号
    _logger->debug("开始请求流程 '{0}' 停止...", this->id());
    _stop_source->request_stop();

    for (auto it = _nodes.rbegin(); it != _nodes.rend(); ++it) {
        auto ref = std::reference_wrapper<IFlowNode>(**it);
        _logger->info("正在停止流程节点：[id={0}, type={1}]", ref.get().id(), ref.get().type());
        co_await ref.get().async_stop();
        _logger->info("流程节点已停止");
    }

    _logger->debug("流程 '{0}' 已停止", this->id());
    co_return;
}

Awaitable<void> Flow::async_send_many(std::vector<std::unique_ptr<Envelope>>&& envelopes) {

    this->_on_send_event(this, envelopes);

    for (auto&& e : envelopes) {
        this->on_pre_route_event()(this, e.get());

        if (e->clone_message) {
            e->msg = std::move(e->msg->clone());
        }
    }

    auto exec = co_await this_coro::executor;

    for (auto&& e : envelopes) {
        // co_await this->async_send_one(std::move(e));
        boost::asio::co_spawn(exec, this->async_send_one_internal(std::move(e)), boost::asio::detached);
    }
    co_return;
}

Awaitable<void> Flow::async_send_one_internal(std::unique_ptr<Envelope> envelope) {

    // 线程池中处理流程
    //
    bool can_deliver = false;
    switch (envelope->destination_node->descriptor()->kind()) {

    case NodeKind::PIPE:
    case NodeKind::SINK:
    case NodeKind::JUNCTION: {
        can_deliver = true;
    } break;

    default:
        auto error_msg = fmt::format("错误的节点类型 [msg_id={0}, source={1}, destination={2}]", envelope->msg->id(),
                                     envelope->source_node->id(), envelope->destination_node->id());
        _logger->error(error_msg);
        throw InvalidDataException(error_msg);
    }

    if (can_deliver) {
        this->on_pre_deliver_event()(this, envelope.get());
        co_await envelope->destination_node->receive_async(envelope->msg);
        this->on_post_deliver_event()(this, envelope.get());
    }
}

IFlowNode* Flow::get_node(const std::string_view id) const {
    for (auto&& n : _nodes) {
        if (n->id() == id) {
            return n.get();
        }
    }
    throw std::runtime_error(fmt::format("找不到节点 ID：{0}", id));
}

}; // namespace edgelink::flows