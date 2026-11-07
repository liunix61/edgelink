// 测试自研协程运行时（替代 Boost.Asio）
#include <edgelink/coroutine.hpp>
#include <cassert>
#include <iostream>

using namespace edgelink;

// 协程：co_await this_coro::executor + co_return 值
Task<int> add_async(int a, int b) {
    auto ex = co_await this_coro::executor();
    co_return a + b;
}

// 协程：co_await 嵌套 task + steady_timer
Task<void> timer_test() {
    auto ex = co_await this_coro::executor();
    SteadyTimer timer(ex, std::chrono::milliseconds(10));
    co_await timer.async_wait();
    co_return;
}

int main() {
    IoContext ctx;
    Executor ex(&ctx);

    // 测试 co_spawn + 嵌套
    bool done = false;
    co_spawn(ex, [&]() -> Task<void> {
        int r = co_await add_async(2, 3);
        assert(r == 5);
        co_await timer_test();
        done = true;
        co_return;
    }(), Detached{});

    // 手动 poll 驱动（替代 run——避免阻塞）
    // 注：timer 用后台线程，需短暂等待
    for (int i = 0; i < 100 && !done; ++i) {
        ctx.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    assert(done);
    std::cout << "COROUTINE RUNTIME TEST PASS" << std::endl;
    return 0;
}
