#pragma once

// edgelink 自研协程运行时（替代 Boost.Asio 协程原语）
// 参考 nanoapi::core::task<T>（已验证零 Boost）设计：
// - task<T>: C++20 协程返回类型（co_await 嵌套 + co_return + 异常传播）
// - co_spawn: 启动协程（fire-and-forget，模仿 asio::co_spawn + detached）
// - this_coro::executor: 当前调度器（thread_local）
// - steady_timer / io_context / signal_set: 事件循环原语
//
// 设计原则：与 Boost.Asio 协程 API 同形（便于上层零改动切换），
//          但底层用 std::coroutine + 自研事件循环（零 Boost）。

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <csignal>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace edgelink {

// ============================ 调度器（Executor） ============================

/// @brief 协程调度器（线程安全的任务队列 + 工作线程）
/// @note 模仿 asio::io_context 的最小实现：投递任务 + 运行循环 + 定时器
class IoContext {
  public:
    explicit IoContext(int concurrency = 1) : _concurrency(concurrency) {}

    IoContext(const IoContext&) = delete;
    IoContext& operator=(const IoContext&) = delete;

    /// @brief 投递一个函数（跨线程安全）
    void post(std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push_back(std::move(fn));
        }
        _cv.notify_one();
    }

    /// @brief 运行循环（阻塞直到 stop）
    void run() {
        _running = true;
        std::vector<std::thread> workers;
        for (int i = 0; i < _concurrency; ++i) {
            workers.emplace_back([this] { worker_loop(); });
        }
        for (auto& w : workers) {
            w.join();
        }
    }

    /// @brief 停止运行循环
    void stop() {
        _running = false;
        _cv.notify_all();
    }

    /// @brief 单次运行（处理一批任务，用于测试）
    void poll() {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_tasks.empty()) {
                return;
            }
            fn = std::move(_tasks.front());
            _tasks.pop_front();
        }
        if (fn) {
            fn();
        }
    }

  private:
    void worker_loop() {
        std::function<void()> fn;
        while (_running) {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this] { return !_tasks.empty() || !_running; });
                if (!_running && _tasks.empty()) {
                    return;
                }
                if (_tasks.empty()) {
                    continue;
                }
                fn = std::move(_tasks.front());
                _tasks.pop_front();
            }
            if (fn) {
                fn();
            }
        }
    }

    std::mutex _mutex;
    std::condition_variable _cv;
    std::deque<std::function<void()>> _tasks;
    std::atomic<bool> _running{false};
    int _concurrency;
};

/// @brief 调度器引用（模仿 asio::any_io_executor 的最小句柄）
/// @note 支持 co_await（asio::this_coro::executor 语义：立即返回自身）
class Executor {
  public:
    Executor() = default;
    explicit Executor(IoContext* ctx) : _ctx(ctx) {}

    void post(std::function<void()> fn) const {
        if (_ctx) {
            _ctx->post(std::move(fn));
        }
    }

    bool operator==(const Executor& other) const { return _ctx == other._ctx; }

    IoContext* context() const { return _ctx; }

    // co_await 支持（asio::this_coro::executor 返回自身，await_ready=true）
    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    Executor await_resume() const { return *this; }

  private:
    IoContext* _ctx = nullptr;
};

// ============================ 协程（task） ============================

/// @brief 协程返回类型（C++20 coroutine）
/// @note initial_suspend = suspend_never（立即启动，模仿 asio::co_spawn 语义）
///       final_suspend 自动恢复 continuation（嵌套 co_await）
template <typename T = void> class Task {
  public:
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {
            struct Awaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation_) {
                        return h.promise().continuation_;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return Awaiter{};
        }

        template <typename U> void return_value(U&& value) noexcept {
            result_ = std::forward<U>(value);
        }
        void unhandled_exception() { exception_ = std::current_exception(); }

        T result_{};
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
    };

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept { return handle_.done(); }
    void await_suspend(std::coroutine_handle<> awaiting) {
        // 惰性 task：被 co_await 时启动（resume 到第一个挂起点），并设 continuation
        handle_.promise().continuation_ = awaiting;
        handle_.resume();
    }
    T await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().result_);
    }
    void resume() {
        if (handle_) {
            handle_.resume();
        }
    }
    bool done() const { return handle_.done(); }

  private:
    std::coroutine_handle<promise_type> handle_;
};

// void 特化
template <> class Task<void> {
  public:
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {
            struct Awaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation_) {
                        return h.promise().continuation_;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return Awaiter{};
        }
        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }

        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
    };

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept { return handle_.done(); }
    void await_suspend(std::coroutine_handle<> awaiting) {
        // 惰性 task：被 co_await 时启动（resume 到第一个挂起点），并设 continuation
        handle_.promise().continuation_ = awaiting;
        handle_.resume();
    }
    void await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }
    void resume() {
        if (handle_) {
            handle_.resume();
        }
    }
    bool done() const { return handle_.done(); }

  private:
    std::coroutine_handle<promise_type> handle_;
};

// ============================ co_spawn / detached ============================

/// @brief detached 标记（模仿 asio::detached）
struct Detached {};

/// @brief 设置当前线程的调度器（co_spawn 时由调用方设置）
inline void set_current_executor(Executor ex) {
    static thread_local Executor* current = nullptr;
    static thread_local std::unique_ptr<Executor> storage;
    if (!current) {
        storage = std::make_unique<Executor>(ex);
        current = storage.get();
    } else {
        *current = ex;
    }
}

/// @brief 获取当前线程的调度器
inline Executor current_executor() {
    static thread_local Executor* current = nullptr;
    static thread_local std::unique_ptr<Executor> storage;
    if (!current) {
        storage = std::make_unique<Executor>(nullptr);
        current = storage.get();
    }
    return *current;
}

/// @brief 启动协程（fire-and-forget，模仿 asio::co_spawn + detached）
/// @note Task 是 suspend_always（惰性）；此函数设置 thread_local executor，
///       然后 resume 启动协程（保证 co_await this_coro::executor 可用）
template <typename T> void co_spawn(Executor executor, Task<T> task, Detached) {
    set_current_executor(executor);
    // 保活：把 task 移入 shared，resume 启动；完成由观察协程/进程退出释放
    auto shared = std::make_shared<Task<T>>(std::move(task));
    shared->resume();
    executor.post([shared] { (void)shared; });
}

/// @brief 启动协程（接受 lambda/函数对象，模仿 asio::co_spawn + detached）
template <typename F> void co_spawn(Executor executor, F&& f, Detached) {
    auto task = f(); // 调用返回 Task/Awaitable
    co_spawn(executor, std::move(task), Detached{});
}

/// @brief 启动协程到 io_context（兼容重载）
template <typename T> void co_spawn(IoContext& ctx, Task<T> task, Detached) {
    co_spawn(Executor(&ctx), std::move(task), Detached{});
}

/// @brief 便捷重载：co_spawn(executor, task, detached) 保活 task
template <typename T> void spawn_keep_alive(Executor executor, Task<T> task) {
    co_spawn(executor, std::move(task), Detached{});
}

// ============================ this_coro ============================

namespace this_coro {
/// @brief 获取当前调度器（thread_local，co_spawn 时已设置）
inline Executor executor() { return current_executor(); }
inline void set_executor(Executor ex) { set_current_executor(ex); }
} // namespace this_coro

// ============================ 定时器 ============================

/// @brief 定时器（模仿 asio::steady_timer，用 std::thread 简化实现）
/// @note 非阻塞：async_wait 投递到后台线程，完成后 post 回调用
class SteadyTimer {
  public:
    explicit SteadyTimer(Executor ex) : _executor(ex) {}
    SteadyTimer(Executor ex, std::chrono::milliseconds ms) : _executor(ex) {
        expires_after(ms);
    }

    void expires_after(std::chrono::milliseconds ms) { _duration = ms; }
    void expires_at(std::chrono::steady_clock::time_point tp) {
        _duration = std::chrono::duration_cast<std::chrono::milliseconds>(tp - std::chrono::steady_clock::now());
    }

    /// @brief 异步等待（返回一个可 co_await 的 awaiter）
    struct WaitAwaiter {
        SteadyTimer* timer;
        bool await_ready() const noexcept { return timer->_duration.count() <= 0; }
        void await_suspend(std::coroutine_handle<> h) const {
            auto ctx = timer->_executor.context();
            auto duration = timer->_duration;
            if (ctx) {
                // 后台线程 sleep 后投递恢复协程
                // 修复：resume 发生在 IoContext worker 线程——该线程的 thread_local executor 未设置，
                //       协程内 `this_coro::executor()` 读到空 executor → 后续 SteadyTimer 用空 ctx 走 else
                //       同步阻塞分支。这里在 resume 前先设置当前线程的 executor，保证协程上下文正确。
                auto ex = timer->_executor;
                std::thread([ctx, h, duration, ex] {
                    std::this_thread::sleep_for(duration);
                    ctx->post([h, ex] {
                        set_current_executor(ex);
                        h.resume();
                    });
                }).detach();
            } else {
                set_current_executor(timer->_executor);
                std::this_thread::sleep_for(duration);
                h.resume();
            }
        }
        void await_resume() const noexcept {}
    };

    WaitAwaiter async_wait() { return WaitAwaiter{this}; }

  private:
    Executor _executor;
    std::chrono::milliseconds _duration{0};
};

// ============================ use_awaitable ============================

/// @brief use_awaitable 标记（兼容 asio::use_awaitable，实际未用——协程直接返回 task）
struct UseAwaitable {};
inline constexpr UseAwaitable use_awaitable{};

// ============================ 信号 ============================

/// @brief 信号集合（模仿 asio::signal_set，用 std::atomic 标志）
/// @note 简化：SIGINT/SIGTERM 处理在 main 用 signal() 注册，此处提供兼容接口
class SignalSet {
  public:
    explicit SignalSet(IoContext&, int sig1 = 0, int sig2 = 0) {
        (void)sig1;
        (void)sig2;
    }
    void async_wait(std::function<void(int, int)> handler) { _handler = std::move(handler); }
    void cancel() {}

  private:
    std::function<void(int, int)> _handler;
};

} // namespace edgelink
