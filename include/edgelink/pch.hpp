#pragma once

#include <cstdlib>
#include <cmath>
#include <cstdint>

#include <memory>

#include <string>
#include <span>
#include <vector>
#include <map>
#include <queue>
#include <variant>
#include <functional>
#include <mutex>
#include <optional>
#include <charconv>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <filesystem>

// 自研替代（零 Boost）：协程运行时 + 信号槽 + 非拷贝基类
#include "edgelink/coroutine.hpp"
#include "edgelink/signal.hpp"
#include "edgelink/common.hpp"

// 去 Boost.Asio/static_string/container/url/json/signals2/lexical_cast
// （逐步替换为自研/标准库实现）

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <rttr/registration>
#include <rttr/type>
#include <spdlog/spdlog.h>

/*
#include "edgelink/edgelink.hpp"
#include "edgelink/engine.hpp"
#include "edgelink/errors.hpp"
#include "edgelink/logging.hpp"
*/
