#pragma once

namespace edgelink::plugins::mqtt {

using Endpoint = async_mqtt::endpoint<async_mqtt::role::client, boost::asio::basic_stream_socket<boost::asio::ip::tcp>>;

struct IMqttBrokerEndpoint {

    virtual bool is_connected() const = 0;

    virtual Awaitable<void> async_publish(const std::string_view topic, const async_mqtt::buffer& payload_buffer,
                                          async_mqtt::qos qos) = 0;

    virtual Awaitable<void> async_publish_string(const std::string_view topic, const std::string_view payload,
                                                 async_mqtt::qos qos) = 0;

};

}; // namespace edgelink::plugins::mqtt
