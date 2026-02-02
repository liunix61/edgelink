#include <edgelink/plugin.hpp>

#include "modbus.hpp"

using namespace edgelink;

namespace edgelink::plugins::modbus {

ModbusClient::ModbusClient(const std::string_view& url, int baud, char parity, int data_bits, int stop_bits)
    : _modbus(nullptr), baud(baud), parity(parity), dataBits(data_bits), stopBits(stop_bits) {
    auto uri = boost::urls::parse_uri(url).value();

    if (uri.scheme() == "tcp") {
        _transport = ModbusTransport::TCP;
        _device = uri.host_address();
    } else if (uri.scheme() == "rtu") {
        _transport = ModbusTransport::RTU;
        _device = uri.host_address();
    } else {
        throw std::exception();
    }
}

ModbusClient::~ModbusClient() {
    //
    this->close();
}

void ModbusClient::connect() {
    _modbus = modbus_new_rtu(_device.c_str(), baud, parity, dataBits, stopBits);
    if (_modbus == nullptr) {
        spdlog::error("创建 modbus 上下文失败");
    }

    if (modbus_connect(_modbus) == -1) {
        std::string error_msg = modbus_strerror(errno);
        spdlog::error("ModBus 连接失败！错误消息：{0}", error_msg);
        modbus_free(_modbus);
        _modbus = nullptr;
        throw ModbusException(error_msg, errno);
    }
}

void ModbusClient::close() noexcept {
    if (_modbus != nullptr) {
        modbus_close(_modbus);
        modbus_free(_modbus);
        _modbus = nullptr;
    }
}

void ModbusClient::read_input_registers(int address, std::span<uint16_t> data) {
    if (modbus_read_input_registers(_modbus, address, data.size(), data.data()) == -1) {
        spdlog::error("ModBus 读取寄存器失败！错误消息：{0}", modbus_strerror(errno));
        throw std::exception();
    }
}

void ModbusClient::write_single_register(int address, uint16_t value) {
    if (modbus_write_register(_modbus, address, value) == -1) {
        spdlog::error("ModBus 写入寄存器失败！错误消息：{0}", modbus_strerror(errno));
        throw std::exception();
    }
}

}; // namespace edgelink::plugins::modbus