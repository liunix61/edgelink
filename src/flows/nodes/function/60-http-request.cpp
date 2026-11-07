// http-request 节点：Node-RED "http request" 兼容实现
// Webhmi 场景：调用本地 REST 桥端点（/api/printer /api/can /api/dds /api/status）
// 原版 Node-RED 的 http request 节点在 PC 端编排、导出 JSON、传输到 webhmi 面板由 EdgeLink 运行
//
// flows.json 节点格式（Node-RED 标准）：
// {
//   "id": "...", "type": "http request", "z": "...",
//   "name": "", "method": "GET", "url": "http://localhost:8080/api/status",
//   "ret": "obj", "paytoqs": "ignore", "tls": "", "persist": false,
//   "x": 300, "y": 300, "wires": [["..."]]
// }
//
// 行为（Node-RED 语义）：
// - 输入 msg：若 msg.url 存在则覆盖节点 url；msg.payload 作为请求体（POST/PUT）
// - 输出 msg：msg.payload = 响应体（JSON 解析）；msg.statusCode = 响应状态码；msg.headers = 响应头
// - 若配置 ret="obj"，响应体解析为 JSON 对象；否则原样字符串

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/url.hpp>

#include "edgelink/edgelink.hpp"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

using namespace edgelink;

/*
    Node-RED http request 节点属性：
    method: GET/POST/PUT/DELETE
    url: 请求地址（可被 msg.url 覆盖）
    ret: "obj"（解析 JSON）| "txt" | "bin"
    paytoqs: "ignore" | "query"（payload 转查询参数）
*/

class HttpRequestNode : public FlowNode {
  public:
    HttpRequestNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config),
          _method(config.if_contains("method") ? std::string(config.at("method").as_string()) : std::string("GET")),
          _url(config.if_contains("url") ? std::string(config.at("url").as_string()) : std::string()),
          _ret(config.if_contains("ret") ? std::string(config.at("ret").as_string()) : std::string("obj")) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        auto executor = co_await boost::asio::this_coro::executor;

        // 解析请求 URL（优先 msg.url，其次节点配置）
        std::string url = _url;
        if (msg->data().contains("url")) {
            url = std::string(msg->data().at("url").as_string());
        }
        if (url.empty()) {
            this->logger()->warn("http request 节点无 url（节点配置与 msg.url 均为空），跳过");
            co_return;
        }

        // 解析 URL（scheme://host:port/path?query）
        auto parsed = boost::urls::parse_uri(url);
        if (!parsed) {
            this->logger()->warn("http request 节点 url 非法（{0}），跳过", url);
            co_return;
        }
        auto u = parsed.value();
        std::string host(u.host());
        std::string port(u.port().empty() ? "80" : std::string(u.port()));
        std::string path(u.path().empty() ? "/" : std::string(u.path()));
        if (u.has_query()) {
            path += "?" + std::string(u.query());
        }

        try {
            // 建立 TCP 连接
            tcp::resolver resolver(executor);
            auto results = co_await resolver.async_resolve(host, port, boost::asio::use_awaitable);

            beast::tcp_stream stream(executor);
            co_await stream.async_connect(results, boost::asio::use_awaitable);

            // 构造 HTTP 请求
            std::string method_str = _method;
            if (msg->data().contains("method")) {
                method_str = std::string(msg->data().at("method").as_string());
            }
            http::verb verb = http::verb::get;
            if (method_str == "POST") {
                verb = http::verb::post;
            } else if (method_str == "PUT") {
                verb = http::verb::put;
            } else if (method_str == "DELETE") {
                verb = http::verb::delete_;
            }

            http::request<http::string_body> req{verb, path, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "EdgeLink/1.0");

            // 请求体：POST/PUT 时取 msg.payload（字符串化）
            if (verb == http::verb::post || verb == http::verb::put || verb == http::verb::patch) {
                if (msg->data().contains("payload")) {
                    auto const& payload = msg->data().at("payload");
                    if (payload.is_string()) {
                        req.body() = std::string(payload.as_string());
                    } else {
                        req.body() = boost::json::serialize(payload);
                    }
                    req.prepare_payload();
                }
            }

            // 发送请求
            http::write(stream, req);

            // 接收响应
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            // 关闭连接
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            // 写入响应到 msg
            auto status = res.result_int();
            auto body = res.body();

            msg->data()["statusCode"] = status;

            // headers：响应头转 JSON 对象
            JsonObject headers_obj;
            for (auto const& field : res.base()) {
                headers_obj[std::string(field.name_string())] = std::string(field.value());
            }
            msg->data()["headers"] = std::move(headers_obj);

            // payload：按 ret 配置解析
            if (_ret == "obj" || _ret == "json") {
                try {
                    msg->data()["payload"] = boost::json::parse(body);
                } catch (const std::exception&) {
                    // 非 JSON 响应：保留字符串
                    msg->data()["payload"] = body;
                }
            } else {
                msg->data()["payload"] = body;
            }

            co_await this->async_send_to_one_port(msg);
        } catch (const std::exception& ex) {
            this->logger()->error("http request 节点请求失败（{0}）：{1}", url, ex.what());
            // 错误时把错误写入 msg.payload 并输出（Node-RED 语义：错误走 catch 节点）
            // 注意：catch 块内不允许 co_await，用 co_spawn 投递发送
            msg->data()["payload"] = std::string(ex.what());
            msg->data()["statusCode"] = -1;
            auto send_future = this->async_send_to_one_port(msg);
            boost::asio::co_spawn(executor, std::move(send_future), boost::asio::detached);
        }
        co_return;
    }

  private:
    const std::string _method;
    const std::string _url;
    const std::string _ret;
};

RTTR_REGISTRATION {
    rttr::registration::class_<FlowNodeProvider<HttpRequestNode, "http request", NodeKind::PIPE>>(
        "edgelink::HttpRequestNodeProvider")
        .constructor()(rttr::policy::ctor::as_raw_ptr);
};
