#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace edgelink {

void init_logging() {

    const std::string filename;
    int size = 20 * 1024 * 1024; // 10M
    int backcount = 5;           // 5

    // create console_sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    // create rotating file sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.txt", size, backcount, true);
    file_sink->set_level(spdlog::level::warn);

    // sink's bucket
    spdlog::sinks_init_list sinks{console_sink, file_sink};

    // create async logger, and use global threadpool
    //spdlog::init_thread_pool(1024 * 8, 1);
    //auto logger = std::make_shared<spdlog::logger>("default_logger", sinks, spdlog::thread_pool());
    //spdlog::initialize_logger(logger);
    // set default logger
    //spdlog::set_default_logger(logger);
    
    spdlog::set_level(spdlog::level::debug);
}

}; // namespace edgelink