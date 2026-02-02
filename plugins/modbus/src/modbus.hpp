#pragma once

namespace edgelink::plugins::modbus {

enum class ModbusTransport {
    RTU = 0,
    TCP = 1,
};

class ModbusException : public edgelink::IOException {
  public:
    ModbusException(const std::string& message, int error_code)
        : edgelink::IOException(message), _error_code(error_code) {}
    ModbusException(const char* message, int error_code) : edgelink::IOException(message), _error_code(error_code) {}
    int error_code() const { return _error_code; }

  private:
    int _error_code;
};

class ModbusClient : public edgelink::IClosable {
  public:
    ModbusClient(const std::string_view& url, int baud, char parity, int data_bits, int stop_bits);
    ~ModbusClient();

    void connect();
    void close() noexcept override;
    void read_input_registers(int address, std::span<uint16_t> data);
    void write_single_register(int address, uint16_t value);

  private:
    modbus_t* _modbus;
    std::string _device;
    int baud;
    char parity;
    int dataBits;
    int stopBits;
    ModbusTransport _transport;
};
}; 