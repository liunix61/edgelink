#include <boost/program_options.hpp>
#include <boost/di.hpp>

#include "version-config.h"
#include "edgelink/edgelink.hpp"
#include "logging.hpp"

#include "app.hpp"

#include "flows/flow.hpp"
#include "flows/flow-factory.hpp"
#include "flows/engine.hpp"
#include "flows/registry.hpp"

using namespace boost;
namespace fs = std::filesystem;
namespace this_coro = boost::asio::this_coro;
namespace po = boost::program_options;

// paho.mqtt.cpp 库客户端是线程安全的，可以多个线程同时访问，但是 set_xxx_callback() 设置的回调禁止阻塞

using namespace edgelink;

void show_banner() {
    fmt::print("EdgeLink 物联网边缘数据采集系统\n");
    fmt::print("版本：{0}, REV: {1}\n", EDGELINK_VERSION, GIT_REVISION);
}

std::string get_home_dir() {
    const char* homePath = nullptr;

    homePath = std::getenv("HOME");

    if (homePath == nullptr) {
        homePath = std::getenv("USERPROFILE");
    }

    if (homePath != nullptr) {
        return std::string(homePath);
    } else {
        auto msg = fmt::format("找不到用户主目录");
        throw IOException(msg);
    }
}

int main(int argc, char* argv[]) {
    show_banner();

    auto exec_path = fs::canonical(std::filesystem::path(argv[0]));

    // flows 路径在 try 外声明（默认 ./flows.json），try 内解析 --flows 覆盖
    std::string flows_path = "./flows.json";

    try {
        po::options_description desc("Allowed options");
        desc.add_options()                                                                   //
            ("help,?", "Produce help message")                                               //
            ("settings,s", po::value<std::string>(), "Path to settings file")                //
            ("flows,f", po::value<std::string>()->default_value("flows.json"), "Flows file") //

#if EL_WITH_WEB_SERVER
            ("port,p", po::value<uint16_t>()->default_value(1990), "port to listen on")      //
#endif

            ("user-dir, u", po::value<std::string>(), "use specified user directory")        //
            ("output-file", po::value<std::string>(), "Output file")                         //
            ("project-id,pid", po::value<std::string>(), "Project ID")                       //
            ("device-id,did", po::value<std::string>(), "Device ID")                         //
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        // 如果命令行中包含 --config 选项，读取配置文件
        if (vm.count("settings")) {
            std::ifstream config_file(vm["settings"].as<std::string>());
            po::store(po::parse_config_file(config_file, desc), vm);
        }

        po::notify(vm);

        if (vm.count("help")) {
            return 0;
        }

        // 解析 flows 路径（--flows / -f），默认 ./flows.json
        if (vm.count("flows")) {
            flows_path = vm["flows"].as<std::string>();
        }
    } catch (const std::exception& e) {
        fmt::print("Error: {0}\n", e.what());
        return 1;
    }

    // 初始化日志系统
    init_logging();

    SPDLOG_INFO("日志子系统已初始化");

    // 若指定了 settings 文件，读取 EdgeLinkSettings 配置（flows 路径等）
    EdgeLinkSettings el_settings{
        .home_path = fs::path("./"),
        .executable_location = exec_path.parent_path(),
        .flows_json_path = flows_path,
    };

    const auto injector =
        di::make_injector(di::bind<>().to(el_settings),                           // EdgeLinkSettings
                          di::bind<App>().in(di::singleton),                      // App
                          di::bind<IEngine>().to<Engine>().in(di::singleton),     // Engine
                          di::bind<IRegistry>().to<Registry>().in(di::singleton), // Registry
                          di::bind<IFlowFactory>().to<edgelink::flows::FlowFactory>().in(di::singleton) // Engine
        );

    // 修复：injector.create<App>() 返回 App 值（非指针），133 行 app-> 非法
    // 改用 create<std::shared_ptr<App>>() 获取共享指针
    auto app = injector.create<std::shared_ptr<App>>();

    // 启动主程序
    try {
        auto nconcurrency = std::thread::hardware_concurrency() + 1;
        asio::io_context io_context(nconcurrency);
        SPDLOG_INFO("系统并发数量：{}", nconcurrency);

        // 干净停机：SIGINT/SIGTERM -> 先停流程引擎（协程 stop_token 传播）再停事件循环
        // app->stop_async() 需要 io_context 在运行，故用 co_spawn 在信号回调里投递停机协程
        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&io_context, app](const boost::system::error_code& ec, int) {
                if (!ec) {
                    SPDLOG_INFO("收到停止信号，开始干净停机...");
                    // 投递异步停机：停引擎协程 -> 停事件循环
                    asio::co_spawn(io_context,
                                   [app]() -> asio::awaitable<void> {
                                       co_await app->stop_async();
                                       co_return;
                                   },
                                   asio::detached);
                }
            });

        asio::co_spawn(io_context, app->run_async(), asio::detached);

        io_context.run();
        SPDLOG_INFO("系统协程系统已停止，开始进行清理...");
        spdlog::shutdown();
    } catch (std::exception& ex) {
        SPDLOG_CRITICAL("程序异常！错误消息：{0}", ex.what());
        spdlog::shutdown();
        return -1;
    }
}
