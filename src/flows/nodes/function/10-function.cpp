
#include <quickjs/quickjs.h>
#include <quickjspp.hpp>

#include "edgelink/edgelink.hpp"

using namespace edgelink;

/*
    {
        "id": "588889e4e95dd46c",
        "type": "function",
        "z": "7c226c13f2e3b224",
        "name": "价值",
        "func": "msg.payload = msg.payload % 100;\nreturn msg;",
        "outputs": 2,
        "noerr": 0,
        "initialize": "// 部署节点后，此处添加的代码将运行一次。 \nvar xxxx = 0;",
        "finalize": "// 节点正在停止或重新部署时，将运行此处添加的代码。 \nvar xxxxx = 0;",
        "libs": [],
        "x": 290,
        "y": 320,
        "wires": [
            [
                "b1c267019d45655a",
                "3596fa8fefd657e4"
            ],
            []
        ]
    }
*/

class EvalEnv final {
  public:
    EvalEnv() {}

    EvalEnv(FlowNode* node) : _node(node) {}

    MsgID generate_msg_id() { return Msg::generate_msg_id(); }

    const std::string_view get_node_id() const { return _node->id(); }
    const std::string_view get_node_name() const { return _node->name(); }
    const unsigned int get_output_count() const { return _node->output_count(); }

    static std::shared_ptr<EvalEnv> create(FlowNode* node) {
        auto ptr = std::make_shared<EvalEnv>(node);
        return ptr;
    }

  private:
    FlowNode* _node;
};

class EvalContext final {
  public:
    EvalContext() {}

    static std::shared_ptr<EvalContext> create(FlowNode* node, std::shared_ptr<Msg> msg) {
        auto ptr = std::make_shared<EvalContext>();
        return ptr;
    }

  private:
    FlowNode* _node;
};

constexpr char JS_PRELUDE[] = R"(
function jsonDeepClone(v) { return JSON.parse(JSON.stringify(v)); }

function cloneMsg(v) {
    var newMsg = jsonDeepClone(v);
    newMsg.id = evalEnv.generateMsgID();
    return newMsg;
}

const node = {
    id: evalEnv.nodeID,
    name: evalEnv.nodeName,
};

/*
const RED = {
    uitl: {
        cloneMessage: function(msg) {
            return cloneMsg(msg);
        },
    }
};
*/

)";

constexpr char JS_USER_FUNC_TEMPLATE[] = R"(
    function __el_user_func(context, msgJsonText) {{
		var msg = JSON.parse(msgJsonText);
        const user_func = function() {{
        /* 用户代码开始 */
        {0};
        /* 用户代码结束 */
        }};
        return JSON.stringify(user_func());
    }};
)";

struct ModuleEntry {
    const std::string module;
    const std::string var;
};

class FunctionNode : public FlowNode {

  public:
    FunctionNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config),
          _outputs(edgelink::value_or(config, "outputs", static_cast<size_t>(1))),
          _func(config.if_contains("func") ? std::string(config.at("func").as_string()) : std::string()),
          _initialize(config.if_contains("initialize") ? std::string(config.at("initialize").as_string())
                                                       : std::string()),
          _finalize(config.if_contains("finalize") ? std::string(config.at("finalize").as_string())
                                                   : std::string()),
          _runtime(), _context(_runtime) {

        if (auto libs_value = config.if_contains("libs")) {
            for (auto const& module_json : libs_value->as_array()) {
                auto const& entry = module_json.as_object();
                auto me = ModuleEntry{
                    std::string(entry.at("module").as_string()),
                    std::string(entry.at("var").as_string()),
                };
                _modules.emplace_back(std::move(me));
            }
        }

        try {
            auto& m = _context.addModule("EdgeLink");

            m.class_<EvalContext>("EvalContext");

            m.class_<EvalEnv>("EvalEnv")
                .fun<&EvalEnv::generate_msg_id>("generateMsgID")
                .property<&EvalEnv::get_node_id>("nodeID")
                .property<&EvalEnv::get_node_name>("nodeName")
                .property<&EvalEnv::get_output_count>("outputCount");

            // 注册全局变量
            _context.global()["evalEnv"] = EvalEnv::create(this);

            // 加载前置代码

            // 加载 EdgeLink 模块
            _context.eval(R"xxx(
            import * as edgeLink from 'EdgeLink';
            globalThis.edgeLink = edgeLink;
        )xxx",
                          "<import>", JS_EVAL_TYPE_MODULE);

            auto prelude_js_path = std::filesystem::current_path() / "resources" / "nodes" / "function" / "prelude.js";
            // 修复：原 evalFile 在 prelude.js 不存在时抛异常导致节点构造崩溃——容错跳过
            if (std::filesystem::exists(prelude_js_path)) {
                _context.evalFile(prelude_js_path.c_str());
            } else {
                this->logger()->warn("function 节点 prelude.js 不存在（{0}），跳过加载", prelude_js_path.string());
            }

            auto js_user_func = fmt::format(JS_USER_FUNC_TEMPLATE, _func);
            _context.eval(js_user_func);

            _user_func_cb =
                static_cast<std::function<const std::string(std::shared_ptr<EvalContext>, const std::string&)>>(
                    _context.eval("__el_user_func"));

        } catch (qjs::exception) {
            auto exc = _context.getException();
            this->logger()->error("QuickJS 错误：{0}", static_cast<const std::string>(exc));
            throw InvalidDataException("function 内置的脚本解析异常");
        }
    }

    Awaitable<void> async_start() override {
        // 修复：原 _context.eval(_initialize) 无 try-catch——initialize JS 异常未捕获导致协程终止
        if (!_initialize.empty()) {
            try {
                _context.eval(_initialize);
            } catch (qjs::exception) {
                auto exc = _context.getException();
                this->logger()->error("function 节点 initialize 脚本异常：{0}", static_cast<const std::string>(exc));
            }
        }
        co_return;
    }

    Awaitable<void> async_stop() override {
        // 修复：原 _context.eval(_finalize) 无 try-catch
        if (!_finalize.empty()) {
            try {
                _context.eval(_finalize);
            } catch (qjs::exception) {
                auto exc = _context.getException();
                this->logger()->error("function 节点 finalize 脚本异常：{0}", static_cast<const std::string>(exc));
            }
        }
        co_return;
    }

    Awaitable<void> receive_async(std::shared_ptr<Msg> msg) override {

        auto eval_ctx = EvalContext::create(this, msg);

        try {

            auto const msg_json_text = msg->to_string();
            const std::string result_json = _user_func_cb(eval_ctx, msg_json_text);

            // 后续处理执行成果
            auto js_result = boost::json::parse(result_json);

            if (js_result.is_null()) {
                // 修复：function 返回 null（Node-RED 合法语义：丢弃消息）——原被当错误
                co_return;
            } else if (js_result.kind() == JsonKind::array) { // 多个端口消息的情况
                auto array = js_result.as_array();
                // 修复：数组含 null 元素（Node-RED 合法：[null, msg] 表示第 0 端口不发）——按对象元素过滤
                std::vector<std::shared_ptr<Msg>> msgs;

                for (auto& msg_json_value : array) {
                    // 直接分发消息，只有是对象的才分发
                    if (msg_json_value.kind() == JsonKind::object) {
                        auto msg_json = msg_json_value.as_object();
                        auto evaled_msg = std::make_shared<Msg>(msg_json, msg->birth_place());
                        msgs.emplace_back(std::move(evaled_msg));
                    }
                }
                if (msgs.size() > this->output_ports().size()) {
                    auto error_msg = "JS 脚本输出错误的端口数";
                    this->logger()->error(error_msg);
                    throw std::out_of_range(error_msg);
                }
                co_await this->async_send_to_many_port(std::forward<std::vector<std::shared_ptr<Msg>>>(msgs));
            } else if (js_result.kind() == JsonKind::object) { // 单个端口消息的情况
                auto object_result = js_result.as_object();
                auto evaled_msg = std::make_shared<Msg>(std::move(object_result), msg->birth_place());
                co_await this->async_send_to_one_port(std::move(evaled_msg));
            } else { // 其他类型不支持
                this->logger()->error("不支持的消息格式：{0}", result_json);
            }
        } catch (qjs::exception) {
            auto exc = _context.getException();
            this->logger()->error("QuickJS 错误：{0}", static_cast<const std::string>(exc));
            // TODO 这里报告错误给 flow
            co_return;
        } catch (std::exception& ex) {
            this->logger()->error("错误：{0}", ex.what());
            throw;
        }
        co_return;
    }

  private:
    const size_t _outputs;
    const std::string _func;
    const std::string _initialize;
    const std::string _finalize;
    unsigned int _noerr;
    std::vector<ModuleEntry> _modules;
    qjs::Runtime _runtime;
    qjs::Context _context;

    std::function<const std::string(std::shared_ptr<EvalContext>, const std::string&)> _user_func_cb;
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<FunctionNode, "function", NodeKind::PIPE>>(
        "edgelink::FunctionNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};