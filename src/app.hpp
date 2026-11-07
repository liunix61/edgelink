#pragma once

#include <atomic>

namespace edgelink {

struct IEngine;

class App : public std::enable_shared_from_this<App> {
  public:
    App(std::shared_ptr<JsonObject> json_config, std::shared_ptr<IEngine> engine) : _engine(engine) {}

    Awaitable<void> run_async();

    Awaitable<void> idle_loop();

    /// @brief 干净停机：停流程引擎，然后停事件循环
    Awaitable<void> stop_async() const;

    /// @brief 请求停机（main 信号处理调用，唤醒 idle_loop 退出）
    void request_stop() { _stop_requested.store(true); }

    /// @brief 引擎访问（供 main 信号处理停机）
    std::shared_ptr<IEngine> engine() const { return _engine; }

  private:
    std::shared_ptr<IEngine> _engine;
    std::atomic<bool> _stop_requested{false};
};

}; //namespace edgelink
