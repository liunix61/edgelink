#include "edgelink/edgelink.hpp"

#include "app.hpp"

namespace edgelink {

using namespace boost;
namespace this_coro = boost::asio::this_coro;

Awaitable<void> App::run_async() {

    auto executor = co_await this_coro::executor;
    // auto self = shared_from_this();

    try {
        co_await _engine->async_start();
    } catch (std::exception& ex) {
        SPDLOG_ERROR("流程引擎启动异常：{0}", ex.what());
        co_return;
    }
    asio::co_spawn(
        executor, [self = this->shared_from_this()] { return self->idle_loop(); }, asio::detached);

    // co_await this->idle_loop();
    co_return;
}

Awaitable<void> App::stop_async() const {
    SPDLOG_INFO("App 停止中...");
    // 先停流程引擎（传播 stop_token 到所有节点协程）
    co_await _engine->async_stop();
    // 再停 idle 循环（cancellation 触发）
    SPDLOG_INFO("App 已停止");
    co_return;
}

Awaitable<void> App::idle_loop() {
    auto executor = co_await this_coro::executor;
    auto cs = co_await boost::asio::this_coro::cancellation_state;
    // 引擎
    SPDLOG_INFO("正在启动 IDLE 协程");
    // 阻塞
    for (;;) {
        if (cs.cancelled() != boost::asio::cancellation_type::none) {
            SPDLOG_INFO("IDLE 协程停止中...");
            break;
        }
        // 协程 IDLE
        asio::steady_timer timer(executor, std::chrono::milliseconds(1000));
        co_await timer.async_wait(asio::use_awaitable);
    }
    SPDLOG_INFO("IDLE 协程已结束");
    co_return;
}

}; // namespace edgelink