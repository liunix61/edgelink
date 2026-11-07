#include "edgelink/edgelink.hpp"

#include "app.hpp"

namespace edgelink {

namespace this_coro = edgelink::this_coro;

Awaitable<void> App::run_async() {

    auto executor = co_await this_coro::executor();
    // auto self = shared_from_this();

    try {
        co_await _engine->async_start();
    } catch (std::exception& ex) {
        SPDLOG_ERROR("流程引擎启动异常：{0}", ex.what());
        co_return;
    }
    co_spawn(
        executor, [self = this->shared_from_this()] { return self->idle_loop(); }, Detached{});

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
    auto executor = co_await this_coro::executor();
    // 引擎
    SPDLOG_INFO("正在启动 IDLE 协程");
    // 阻塞
    for (;;) {
        // 简化：cancellation_state 用 _stop_requested 原子标志（stop_async 由 main 信号触发）
        if (_stop_requested.load()) {
            SPDLOG_INFO("IDLE 协程停止中...");
            break;
        }
        // 协程 IDLE
        SteadyTimer timer(executor, std::chrono::milliseconds(1000));
        co_await timer.async_wait();
    }
    SPDLOG_INFO("IDLE 协程已结束");
    co_return;
}

}; // namespace edgelink
