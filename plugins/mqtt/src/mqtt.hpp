#pragma once

#include <string_view>
#include <cstdint>

namespace edgelink::plugins::mqtt {

// 去 Boost：MQTT 客户端改用 EMQX NanoSDK（nng，纯 C 零 Boost）
// async_mqtt::endpoint / buffer / qos 全部移除，接口改用标准类型
struct IMqttBrokerEndpoint {

    virtual bool is_connected() const = 0;

    // 发布二进制负载（payload 字节 + 长度），qos: 0/1/2
    virtual Awaitable<void> async_publish(const std::string_view topic, const uint8_t* payload,
                                          size_t payload_len, uint8_t qos) = 0;

    // 发布字符串负载
    virtual Awaitable<void> async_publish_string(const std::string_view topic, const std::string_view payload,
                                                 uint8_t qos) = 0;

    // 订阅主题（mqtt in 节点用 broker 的 socket 订阅）
    virtual Awaitable<void> async_subscribe(const std::string_view topic, uint8_t qos) = 0;

    // 设置消息回调（收到 PUBLISH 时调用，arg 为回调上下文）
    virtual void set_message_callback(void (*cb)(const std::string_view topic, const uint8_t* payload,
                                                 size_t len, void* arg),
                                      void* arg) = 0;
};

}; // namespace edgelink::plugins::mqtt
