#include "edgelink/edgelink.hpp"

#include <boost/mustache.hpp>

using namespace edgelink;

class TemplateNode : public FlowNode {

  public:
    TemplateNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config), _field(config.at("field").as_string()), // .field 属性
          _field_type(config.at("fieldType").as_string()),                          // .fieldType 属性
          _template(config.at("template").as_string())

    {
        //
    }

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {

        JsonValue wrapped_msg = JsonObject({
            {_field, JsonValue(msg->data().at(_field))},
        });
        std::stringstream out;
        boost::mustache::render(_template, out, wrapped_msg, {});

        auto json_text = out.str();
        auto parsed_msg_field_value = boost::json::parse(json_text);

        msg->data()[_field] = parsed_msg_field_value;

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