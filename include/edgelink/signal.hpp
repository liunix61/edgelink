#pragma once

// edgelink 自研信号槽（替代 boost::signals2，零 Boost）
// 支持：
// - 同步 void 信号（Flow* 事件）
// - 协程返回类型信号（Awaitable<void>，预留）
// - connect / operator() 发射 / disconnect

#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "edgelink/common.hpp"

namespace edgelink {

/// @brief 同步信号槽（替代 boost::signals2::signal 最小实现）
/// @tparam TFuncType 函数签名（如 void(IFlow*, Envelope*)）
template <typename TFuncType> class Signal;

/// @brief 信号连接句柄
class SignalConnection {
  public:
    SignalConnection() = default;
    SignalConnection(std::shared_ptr<bool> alive) : _alive(std::move(alive)) {}
    void disconnect() {
        if (_alive) {
            *_alive = false;
        }
    }

  private:
    std::shared_ptr<bool> _alive;
};

/// @brief void 返回的同步信号（最常用）
template <typename... Args> class Signal<void(Args...)> {
  public:
    using SlotType = std::function<void(Args...)>;

    /// @brief 连接槽，返回连接句柄
    SignalConnection connect(SlotType slot) {
        auto alive = std::make_shared<bool>(true);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _slots.emplace_back(std::make_pair(alive, std::move(slot)));
        }
        return SignalConnection(alive);
    }

    /// @brief 发射信号（调用所有存活槽）
    void operator()(Args... args) {
        std::vector<SlotType> to_call;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            // 先清理已断开的槽，再收集存活槽
            auto it = _slots.begin();
            while (it != _slots.end()) {
                if (!*it->first) {
                    it = _slots.erase(it);
                } else {
                    to_call.push_back(it->second);
                    ++it;
                }
            }
        }
        for (auto& slot : to_call) {
            slot(args...);
        }
    }

    /// @brief 清空所有连接
    void disconnect_all() {
        std::lock_guard<std::mutex> lock(_mutex);
        _slots.clear();
    }

  private:
    std::mutex _mutex;
    std::vector<std::pair<std::shared_ptr<bool>, SlotType>> _slots;
};

/// @brief 协程返回类型的信号（预留；Awaitable<void> 返回）
/// @note 由于 Awaitable 是移动语义，发射需收集协程并逐个 co_await；
///       实际当前源码未在关键路径 emit 协程 signal，提供类型兼容。
template <typename... Args> class Signal<Awaitable<void>(Args...)> {
  public:
    using SlotType = std::function<Awaitable<void>(Args...)>;

    SignalConnection connect(SlotType slot) {
        auto alive = std::make_shared<bool>(true);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _slots.emplace_back(std::make_pair(alive, std::move(slot)));
        }
        return SignalConnection(alive);
    }

    /// @brief 同步发射（收集所有协程并立即驱动到第一个挂起点）
    void operator()(Args... args) {
        std::vector<SlotType> to_call;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _slots.begin();
            while (it != _slots.end()) {
                if (!*it->first) {
                    it = _slots.erase(it);
                } else {
                    to_call.push_back(it->second);
                    ++it;
                }
            }
        }
        for (auto& slot : to_call) {
            // 启动协程（Task suspend_always，需 resume 驱动）
            auto task = slot(args...);
            task.resume();
        }
    }

    void disconnect_all() {
        std::lock_guard<std::mutex> lock(_mutex);
        _slots.clear();
    }

  private:
    std::mutex _mutex;
    std::vector<std::pair<std::shared_ptr<bool>, SlotType>> _slots;
};

} // namespace edgelink
