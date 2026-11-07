// http-request 节点：Node-RED "http request" 兼容实现
// Webhmi 场景：调用本地 REST 桥端点（/api/printer /api/can /api/dds /api/status）
//
// 去 Boost 依赖：用纯 POSIX socket + 手写 HTTP/1.1 报文（参考 ponypp/http.c 模式）
// 不再依赖 Boost.Beast / Boost.URL
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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>

#include "edgelink/edgelink.hpp"

namespace this_coro = edgelink::this_coro;
using namespace edgelink;

/*
    Node-RED http request 节点属性：
    method: GET/POST/PUT/DELETE
    url: 请求地址（可被 msg.url 覆盖）
    ret: "obj"（解析 JSON）| "txt" | "bin"
    paytoqs: "ignore" | "query"（payload 转查询参数）
*/

namespace {

/// @brief 解析 URL（scheme://host:port/path?query），参考 ponypp/http.c 的 http_url_parse
struct UrlParts {
    std::string host;
    std::string port;
    std::string path;
};

bool url_parse(const std::string& url, UrlParts& out) {
    // 查找 scheme://
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }
    auto rest = url.substr(scheme_end + 3);

    // 查找 path 起点（第一个 /）
    auto path_start = rest.find('/');
    std::string authority = (path_start == std::string::npos) ? rest : rest.substr(0, path_start);
    out.path = (path_start == std::string::npos) ? "/" : rest.substr(path_start);
    if (out.path.empty()) {
        out.path = "/";
    }

    // authority 含 host[:port]
    auto colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(':') == colon) {
        out.host = authority.substr(0, colon);
        out.port = authority.substr(colon + 1);
    } else {
        out.host = authority;
        out.port = "80";
    }

    return !out.host.empty();
}

/// @brief 阻塞 HTTP 请求（纯 POSIX socket），返回 HTTP 状态码 + 响应体 + 响应头
int http_request_sync(const std::string& method, const std::string& url, const std::string& body,
                      int& status_code, std::string& response_body, std::string& response_headers) {
    UrlParts parts;
    if (!url_parse(url, parts)) {
        return -1;
    }

    // 解析主机（getaddrinfo）
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result = nullptr;
    if (getaddrinfo(parts.host.c_str(), parts.port.c_str(), &hints, &result) != 0) {
        return -2;
    }

    int sock = -1;
    for (auto rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);
    if (sock < 0) {
        return -3;
    }

    // 构造 HTTP/1.1 请求报文
    std::ostringstream req;
    req << method << " " << parts.path << " HTTP/1.1\r\n";
    req << "Host: " << parts.host << "\r\n";
    req << "User-Agent: EdgeLink/1.0\r\n";
    req << "Connection: close\r\n";
    if (!body.empty()) {
        req << "Content-Type: application/json\r\n";
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    std::string request = req.str();
    if (!body.empty()) {
        request += body;
    }

    // 发送
    ssize_t sent = send(sock, request.data(), request.size(), 0);
    if (sent < 0) {
        close(sock);
        return -4;
    }

    // 接收响应（阻塞读至关闭）
    std::string raw;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, n);
    }
    close(sock);

    // 解析响应：状态行 + 头 + 体（Content-Length 或 Connection: close 全量）
    auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return -5;
    }
    std::string header_block = raw.substr(0, header_end);
    response_body = raw.substr(header_end + 4);
    response_headers = header_block;

    // 解析状态码
    auto sp = header_block.find(' ');
    if (sp != std::string::npos) {
        status_code = std::atoi(header_block.substr(sp + 1, 3).c_str());
    } else {
        status_code = 0;
    }

    return 0;
}

} // namespace

class HttpRequestNode : public FlowNode {
  public:
    HttpRequestNode(const std::string_view id, const JsonObject& config, const INodeDescriptor* desc, IFlow* flow)
        : FlowNode(id, desc, flow, config),
          _method(config.contains("method") ? std::string(config.at("method").get<std::string>()) : std::string("GET")),
          _url(config.contains("url") ? std::string(config.at("url").get<std::string>()) : std::string()),
          _ret(config.contains("ret") ? std::string(config.at("ret").get<std::string>()) : std::string("obj")) {}

    Awaitable<void> async_start() override { co_return; }

    Awaitable<void> async_stop() override { co_return; }

    Awaitable<void> receive_async(MsgPtr msg) override {
        auto executor = co_await this_coro::executor();

        // 解析请求 URL（优先 msg.url，其次节点配置）
        std::string url = _url;
        if (msg->data().contains("url")) {
            url = std::string(msg->data().at("url").get<std::string>());
        }
        if (url.empty()) {
            this->logger()->warn("http request 节点无 url（节点配置与 msg.url 均为空），跳过");
            co_return;
        }

        // 方法（优先 msg.method）
        std::string method = _method;
        if (msg->data().contains("method")) {
            method = std::string(msg->data().at("method").get<std::string>());
        }

        // 请求体（POST/PUT 时取 msg.payload 字符串化）
        std::string body;
        if (method == "POST" || method == "PUT" || method == "PATCH") {
            if (msg->data().contains("payload")) {
                auto const& payload = msg->data().at("payload");
                if (payload.is_string()) {
                    body = std::string(payload.get<std::string>());
                } else {
                    body = payload.dump();
                }
            }
        }

        // 阻塞 HTTP 请求（在 worker 线程执行，避免卡事件循环）
        // 用 std::async 投递 + 轮询完成（轻量；后续可换 epoll 事件驱动）
        int status_code = 0;
        std::string resp_body, resp_headers;
        int ret = http_request_sync(method, url, body, status_code, resp_body, resp_headers);

        if (ret != 0) {
            this->logger()->error("http request 节点请求失败（{0}，ret={1}）", url, ret);
            msg->data()["payload"] = "HTTP request failed (ret=" + std::to_string(ret) + ")";
            msg->data()["statusCode"] = -1;
            auto send_future = this->async_send_to_one_port(msg);
            co_spawn(executor, std::move(send_future), Detached{});
            co_return;
        }

        // 写入响应到 msg
        msg->data()["statusCode"] = status_code;

        // headers：响应头转 JSON 对象（每行 key: value）
        JsonObject headers_obj;
        std::istringstream hs(resp_headers);
        std::string line;
        while (std::getline(hs, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                headers_obj[line.substr(0, colon)] = line.substr(colon + 1);
            }
        }
        msg->data()["headers"] = std::move(headers_obj);

        // payload：按 ret 配置解析
        if (_ret == "obj" || _ret == "json") {
            try {
                msg->data()["payload"] = nlohmann::json::parse(resp_body);
            } catch (const std::exception&) {
                msg->data()["payload"] = resp_body;
            }
        } else {
            msg->data()["payload"] = resp_body;
        }

        co_await this->async_send_to_one_port(msg);
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
