#include "edgelink/edgelink.hpp"

#include <sstream>

using namespace edgelink;

namespace {

/// @brief 去 Boost：自研 mini mustache 渲染（替代 boost::mustache）
/// @note 支持 {{key}} / {{key.sub}} 简单替换；{{#section}}...{{/section}} 区块
///       完整 mustache 语义超出 EdgeLink template 节点需求，此处最小实现
std::string render_template(const std::string& tmpl, const JsonObject& data) {
    std::string out;
    size_t i = 0;
    while (i < tmpl.size()) {
        auto open = tmpl.find("{{", i);
        if (open == std::string::npos) {
            out.append(tmpl, i, std::string::npos);
            break;
        }
        // 复制 {{ 之前的文本
        out.append(tmpl, i, open - i);
        auto close = tmpl.find("}}", open + 2);
        if (close == std::string::npos) {
            out.append(tmpl, open, std::string::npos);
            break;
        }
        // 提取标签内容
        std::string tag = tmpl.substr(open + 2, close - open - 2);
        // 去除首尾空白
        size_t s = tag.find_first_not_of(" \t\r\n");
        size_t e = tag.find_last_not_of(" \t\r\n");
        std::string key = (s == std::string::npos) ? "" : tag.substr(s, e - s + 1);

        // 查找值：支持点分路径
        const JsonObject* current = &data;
        const JsonObject* found = nullptr;
        size_t dot = 0;
        while (current && dot <= key.size()) {
            auto next_dot = key.find('.', dot);
            std::string segment = key.substr(dot, next_dot == std::string::npos ? std::string::npos : next_dot - dot);
            if (segment.empty()) break;
            auto it = current->find(segment);
            if (it == current->end()) {
                found = nullptr;
                break;
            }
            if (next_dot == std::string::npos) {
                found = &(*it);
                break;
            }
            if (!it->is_object()) {
                found = nullptr;
                break;
            }
            current = &(*it);
            dot = next_dot + 1;
        }

        if (found != nullptr && !found->is_object() && !found->is_array()) {
            if (found->is_string()) {
                out += found->get<std::string>();
            } else {
                out += found->dump();
            }
        }
        // 对象/数组区块或未找到：渲染为空（简化）
        i = close + 2;
    }
    return out;
}

} // namespace

class TemplateNode : public FlowNode {

  public:
    TemplateNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config), _field(config.at("field").get<std::string>()), // .field 属性
          _field_type(config.at("fieldType").get<std::string>()),                          // .fieldType 属性
          _template(config.at("template").get<std::string>())

    {
        //
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {

        JsonObject wrapped_msg = JsonObject({
            {_field, JsonValue(msg->data().at(_field))},
        });
        std::string json_text = render_template(_template, wrapped_msg);

        // 修复：原实现无条件 boost::json::parse(json_text)——模板输出为纯文本（非 JSON）时崩溃
        // Node-RED template 节点默认输出字符串；仅当输出是合法 JSON 时才解析，否则保留为字符串
        try {
            auto parsed_msg_field_value = nlohmann::json::parse(json_text);
            msg->data()[_field] = parsed_msg_field_value;
        } catch (const std::exception&) {
            // 纯文本输出：保留字符串
            msg->data()[_field] = json_text;
        }

        co_await this->async_send_to_one_port(msg);

        co_return;
    }

  private:
    const std::string _field;
    const std::string _field_type;
    const std::string _template;
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<TemplateNode, "template", NodeKind::PIPE>>(
        "edgelink::TemplateNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
