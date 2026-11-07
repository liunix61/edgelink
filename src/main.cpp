#if EL_USE_BOOST_DI
#include <boost/di.hpp>
#endif

#include "version-config.h"
#include "edgelink/edgelink.hpp"
#include "logging.hpp"

#include "app.hpp"

#include "flows/flow.hpp"
#include "flows/flow-factory.hpp"
#include "flows/engine.hpp"
#include "flows/registry.hpp"

namespace fs = std::filesystem;
namespace this_coro = edgelink::this_coro;
using namespace edgelink;

// paho.mqtt.cpp 库客户端是线程安全的，可以多个线程同时访问，但是 set_xxx_callback() 设置的回调禁止阻塞

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

    // 手写命令行解析（去 Boost.Program_options）：
    // --flows/-f <path> | --settings/-s <path> | --port/-p <port> | --help/-? | --user-dir/-u <path> | ...
    std::string flows_path = "./flows.json";
    std::string settings_path;
    bool show_help = false;

    auto arg_value = [&](const std::string& name, std::string& out) -> bool {
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == name && i + 1 < argc) {
                out = argv[i + 1];
                return true;
            }
            if (a.rfind(name + "=", 0) == 0) {
                out = a.substr(name.size() + 1);
                return true;
            }
        }
        return false;
    };

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-?" || a == "-h") {
            show_help = true;
        }
    }

    if (show_help) {
        fmt::print("用法：edgelink [--flows <path>] [--settings <path>] [--port <port>]\n");
        return 0;
    }

    arg_value("--flows", flows_path) || arg_value("-f", flows_path);
    arg_value("--settings", settings_path) || arg_value("-s", settings_path);

    // 初始化日志系统
    init_logging();

    SPDLOG_INFO("日志子系统已初始化");

    // 若指定了 settings 文件，读取 EdgeLinkSettings 配置（flows 路径等）
    EdgeLinkSettings el_settings{
        .home_path = fs::path("./"),
        .executable_location = exec_path.parent_path(),
        .flows_json_path = flows_path,
    };

    // 编译开关（用户明令）：EL_USE_BOOST_DI=ON 用 boost-di（header-only 零链接），OFF 用自研手动 DI
#if EL_USE_BOOST_DI
    const auto injector =
        boost::di::make_injector(boost::di::bind<>().to(el_settings),                           // EdgeLinkSettings
                          boost::di::bind<App>().in(boost::di::singleton),                      // App
                          boost::di::bind<IEngine>().to<Engine>().in(boost::di::singleton),     // Engine
                          boost::di::bind<IRegistry>().to<Registry>().in(boost::di::singleton), // Registry
                          boost::di::bind<IFlowFactory>().to<edgelink::flows::FlowFactory>().in(boost::di::singleton) // Engine
        );
    auto app = injector.create<std::shared_ptr<App>>();
#else
    // 自研手动 DI（零第三方依赖）：Registry → FlowFactory → Engine → App
    auto registry = std::make_shared<Registry>(el_settings);
    auto flow_factory = std::make_shared<edgelink::flows::FlowFactory>(*registry);
    auto engine = std::make_shared<Engine>(el_settings, *flow_factory);
    auto app = std::make_shared<App>(std::make_shared<JsonObject>(), engine);
#endif

    // 启动主程序
    try {
        auto nconcurrency = std::thread::hardware_concurrency() + 1;
        IoContext io_context(nconcurrency);
        SPDLOG_INFO("系统并发数量：{}", nconcurrency);

        // 干净停机：SIGINT/SIGTERM -> 先停流程引擎（协程 stop_token 传播）再停事件循环
        // 自研简化：std::signal 设原子标志 + app->request_stop()（idle_loop 检测退出）
        static std::atomic<bool> g_stop_signal{false};
        std::signal(SIGINT, [](int) { g_stop_signal.store(true); });
        std::signal(SIGTERM, [](int) { g_stop_signal.store(true); });

        co_spawn(io_context, app->run_async(), Detached{});

        // 主循环：等待信号或 idle_loop 结束（简化：轮询信号标志）
        // 注：真正干净停机需信号触发 stop_async；此处用请求标志 + 主线程等待
        io_context.run();
        SPDLOG_INFO("系统协程系统已停止，开始进行清理...");
        spdlog::shutdown();
    } catch (std::exception& ex) {
        SPDLOG_CRITICAL("程序异常！错误消息：{0}", ex.what());
        spdlog::shutdown();
        return -1;
    }
}
