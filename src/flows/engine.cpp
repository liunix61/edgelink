#include "edgelink/edgelink.hpp"
#include "edgelink/flows/dependency-sorter.hpp"
#include "./engine.hpp"

using namespace boost;
namespace this_coro = boost::asio::this_coro;

namespace edgelink {

Engine::Engine(const EdgeLinkSettings& el_config, const IFlowFactory& flow_factory)
    : _logger(spdlog::default_logger()->clone("Engine")), _settings(el_config), _flow_factory(flow_factory),
      _flows_json_path(el_config.flows_json_path) {

    // std::vector<std::unique_ptr<IFlow>> create_flows(const boost::json::array& flows_config);
}

Engine::~Engine() {
    //
    // TODO 同步关闭
    _logger->info("流程引擎清理中...");

    asio::io_context io_context(1);
    asio::co_spawn(io_context, this->async_stop(), asio::detached);
    io_context.run();

    _logger->info("流程引擎已关闭");
}

Awaitable<void> Engine::async_start() {

    auto executor = co_await boost::asio::this_coro::executor;
    // TODO 检查是否在运行

    _logger->info("流程引擎 > 开始加载流配置：'{0}'", _flows_json_path);

    _flows.clear();

    std::ifstream flows_file(_flows_json_path);
    auto flows_config =
        boost::json::parse(flows_file, {}, {.allow_comments = true, .allow_trailing_commas = true}).as_array();

    if (flows_config.size() == 0) {
        throw BadFlowConfigException("There are no node in the configuration file of the flows!");
    }

    auto global_nodes = _flow_factory.create_global_nodes(flows_config, this);
    for (auto& gn : global_nodes) {
        _global_nodes.emplace_back(std::move(gn));
    }

    auto flows = _flow_factory.create_flows(flows_config, this);
    for (auto& flow : flows) {
        _flows.emplace_back(std::move(flow));
    }

    //
    _logger->info("开始启动流程引擎");
    _stop_source = std::make_unique<std::stop_source>();

    for (auto& node : _global_nodes) {
        _logger->debug("正在启动全局节点：{0}", node->id());
        boost::asio::co_spawn(executor, node->async_start(), boost::asio::detached);
    }

    for (auto& flow : _flows) {
        _logger->debug("正在启动流程：{0}", flow->id());
        boost::asio::co_spawn(executor, flow->async_start(), boost::asio::detached);
    }
    _logger->info("流程引擎已启动");
}

Awaitable<void> Engine::async_stop() {
    // 给出线程池停止信号
    _logger->info("开始请求流程引擎停止...");
    _stop_source->request_stop();

    for (auto it = _flows.rbegin(); it != _flows.rend(); ++it) {
        auto ref = std::reference_wrapper<IFlow>(**it); // 使用 std::reference_wrapper
        co_await ref.get().async_stop();
    }

    for (auto it = _global_nodes.rbegin(); it != _global_nodes.rend(); ++it) {
        auto ref = std::reference_wrapper<IStandaloneNode>(**it); // 使用 std::reference_wrapper
        _logger->debug("正在停止全局节点：[id={0}, type={1}]", ref.get().id(), ref.get().type());
        co_await ref.get().async_stop();
        _logger->debug("全局节点已停止");
    }

    _logger->info("流程引擎已停止");
    co_return;
}

}; // namespace edgelink