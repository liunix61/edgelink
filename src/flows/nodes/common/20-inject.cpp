
#include <croncpp.h>

#include "edgelink/edgelink.hpp"
#include "edgelink/propex.hpp"

namespace this_coro = edgelink::this_coro;

using namespace edgelink;

/*
   {
    "id": "00c80d77c0c5a9de",
    "type": "inject",
    "z": "73e0fcd142fc5256",
    "name": "",
    "props": [
        {
            "p": "payload"
        },
        {
            "p": "topic",
            "vt": "str"
        },
        {
            "p": "123",
            "v": "123",
            "vt": "flow"
        }
    ],
    "repeat": "",
    "crontab": "",
    "once": false,
    "onceDelay": 0.1,
    "topic": "",
    "payload": "",
    "payloadType": "date",
    "x": 270,
    "y": 80,
    "wires": [
        []
    ]
}
*/

struct PropertyEntry {
    std::string p;
    std::optional<std::string> vt;
    std::optional<JsonValue> v;

    PropertyEntry(const std::string_view p, const std::string_view vt, const JsonValue& v)
        : p(p), vt(vt), v(v) {
        //
    }

    explicit PropertyEntry(const JsonObject& obj) {
        this->p = std::string(obj.at("p").get<std::string>());

        if (auto it = obj.find("vt"); it != obj.end()) {
            this->vt = it->get<std::string>();
        }

        if (auto it = obj.find("v"); it != obj.end()) {
            this->v = *it;
        }
    }
};

class InjectNode : public SourceNode {
  public:
    InjectNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : SourceNode(id, desc, flow, config), _once(config.at("once").get<bool>()) {

        if (auto it = config.find("props"); it != config.end()) {
            for (auto const& prop : *it) {
                auto const& entry = prop;
                PropertyEntry pe(entry);
                _props.emplace_back(std::move(pe));
            }
        }

        /* Handle legacy */
        if (config.contains("props")) {
            for (size_t i = 0, l = _props.size(); i < l; i++) {
                if (_props.at(i).p == "payload" && !_props[i].v) {
                    _props[i].v = config.at("payload");
                    _props[i].vt = std::string(config.at("payloadType").get<std::string>());
                } else if (_props.at(i).p == "topic" && _props.at(i).vt == "str" && !_props.at(i).v) {
                    _props[i].v = config.at("topic");
                }
            }
        } else {
            _props.emplace_back(
                PropertyEntry("payload", std::string(config.at("payloadType").get<std::string>()), config.at("payload")));

            if (auto it = config.find("topic"); it != config.end()) {
                _props.emplace_back(PropertyEntry("topic", "str", *it));
            }
        }

        if (auto it = config.find("repeat"); it != config.end()) {
            const std::string repeat_str = it->get<std::string>();
            if (!repeat_str.empty()) {
                try {
                    _repeat = (uint64_t)std::floor(std::stod(std::string(repeat_str)) * 1000);
                } catch (const std::exception& ex) {
                    this->logger()->warn("inject 节点 repeat 值非法（{0}）：{1}", repeat_str, ex.what());
                    _repeat.reset();
                }
            }
        }

        if (auto it = config.find("crontab"); it != config.end()) {
            const std::string crontab_str = it->get<std::string>();
            if (!crontab_str.empty()) {
                try {
                    _cron = ::cron::make_cron(crontab_str);
                } catch (const std::exception& ex) {
                    this->logger()->warn("inject 节点 crontab 值非法（{0}）：{1}", crontab_str, ex.what());
                    _cron.reset();
                }
            }
        }

        if (auto it = config.find("onceDelay"); it != config.end()) {
            double once_delay = 0.1;
            if (it->is_number()) {
                once_delay = it->get<double>();
            } else if (it->is_string()) {
                const auto str = it->get<std::string>();
                if (!str.empty()) {
                    try {
                        once_delay = std::stod(str);
                    } catch (const std::exception&) {
                        once_delay = 0.1;
                    }
                }
            }
            if (once_delay != NAN && once_delay > 0) {
                _once_delay = (uint64_t)std::floor(once_delay * 1000);
            }
        }
    }

  protected:
    Awaitable<void> on_async_run() override {
        auto executor = co_await this_coro::executor();
        this->logger()->info("inject on_async_run 执行：once={}, repeat={}, cron={}", _once, _repeat ? *_repeat : 0,
                             _cron ? true : false);

        if (_once) {
            this->logger()->info("inject once 分支：执行 async_once_task");
            co_await this->async_once_task(executor);
        }

        // 进入循环执行
        if (_repeat && *_repeat > 0) {
            co_await this->async_repeat_task(executor);
        } else if (_cron) {
            co_await this->async_cron_task(executor);
        } else if (_once) {
            // 已经在 async_start 分支处理，此处防御性返回
            co_return;
        } else {
            // 既无 repeat 也无 cron 也无 once：无触发源，仅日志警告不抛异常
            this->logger()->warn("inject 节点无有效触发配置（无 repeat/cron/once），节点不发送消息");
            co_return;
        }

        co_return;
    }

    Awaitable<void> async_stop() override {
        // 请求停止循环（repeat/cron 无限循环检查此标志退出）
        _stop_requested.store(true);
        co_return;
    }

  private:
    std::shared_ptr<Msg> create_msg() {
        auto msg = std::make_shared<Msg>(this);

        for (auto const& prop : _props) {
            auto parsed_value = propex::evaluate_property_value(prop.v.value(), prop.vt.value(), *this, *msg);
            msg->data()[prop.p] = std::move(parsed_value);
        }

        return msg;
    }

    Awaitable<void> async_once_task(Executor executor) {
        auto msg = this->create_msg();
        co_await this->async_send_to_one_port(std::move(msg));
        co_return;
    }

    Awaitable<void> async_cron_task(Executor executor) {
        while (!_stop_requested.load()) {
            std::time_t now = std::time(0);
            std::time_t next = ::cron::cron_next(*_cron, now);
            auto sleep_time = (next - now);
            // 防御：cron 表达式无后续触发点（cron_next 返回 0 或过期）——避免忙循环
            if (sleep_time <= 0) {
                this->logger()->warn("inject 节点 cron 表达式无有效触发点，停止触发");
                break;
            }

            SteadyTimer timer(executor, std::chrono::milliseconds(sleep_time));
            co_await timer.async_wait();

            if (_stop_requested.load()) {
                break;
            }
            auto msg = this->create_msg();
            co_await this->async_send_to_one_port(std::move(msg));
        }
        co_return;
    }

    Awaitable<void> async_repeat_task(Executor executor) {

        while (!_stop_requested.load()) {
            // 防御：repeat 周期为 0 会导致忙循环（毫秒定时器 0 间隔）——跳过无效周期
            if (*_repeat == 0) {
                this->logger()->warn("inject 节点 repeat 周期为 0，跳过本次触发");
                break;
            }
            SteadyTimer timer(executor, std::chrono::milliseconds(*_repeat));
            co_await timer.async_wait();

            if (_stop_requested.load()) {
                break;
            }
            auto msg = this->create_msg();
            co_await this->async_send_to_one_port(std::move(msg));
        }
        co_return;
    }

  private:
    // 各个字段
    std::optional<uint64_t> _repeat;
    std::optional<cron::cronexpr> _cron;
    bool _once;
    std::optional<uint64_t> _once_delay;
    std::vector<PropertyEntry> _props;
    std::atomic<bool> _stop_requested{false};
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<InjectNode, "inject", NodeKind::SOURCE>>("edgelink::InjectNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
