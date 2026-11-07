#pragma once

#include <edgelink/coroutine.hpp>
#include <edgelink/export.hpp>
#include <memory>

namespace edgelink {

struct EDGELINK_EXPORT IClosable {
    virtual void close() noexcept = 0;
};

using Bytes = std::vector<uint8_t>;

// Noncopyable 用标准库模式替代 boost::noncopyable
class Noncopyable {
  protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

// Awaitable 用自研协程替代 boost::asio::awaitable（零 Boost）
template <typename T> using Awaitable = edgelink::Task<T>;

}; // namespace edgelink
