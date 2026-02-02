#pragma once

namespace edgelink {

class NotSupportedException : public std::logic_error {
  public:
    explicit NotSupportedException(const std::string str) : std::logic_error(str) {}
    explicit NotSupportedException(const char* str) : std::logic_error(str) {}
    NotSupportedException(NotSupportedException&& other) noexcept : std::logic_error(std::move(other)) {}
};

class NotImplementedException : public std::logic_error {
  public:
    explicit NotImplementedException(const std::string str) : std::logic_error(str) {}
    explicit NotImplementedException(const char* str) : std::logic_error(str) {}
    NotImplementedException(NotImplementedException&& other) noexcept : std::logic_error(std::move(other)) {}
};

#define TODO(msg) (throw NotImplementedException(msg))

class ArgumentException : public std::invalid_argument {};

class IOException : public std::exception {
  public:
    explicit IOException(const std::string& message) : _message(message) {}
    explicit IOException(const char* message) : _message(message) {}

    // 重载 std::exception 的 what 函数，返回异常的描述信息
    const char* what() const noexcept override { return _message.c_str(); }

  private:
    const std::string _message;
};

class BadConfigException : public std::exception {
  public:
    BadConfigException(const std::string& key, const std::string& message) : _message(message), _key(key) {}
    const char* what() const noexcept override { return _message.c_str(); }
    const std::string_view key() const noexcept { return _key.c_str(); }

  private:
    std::string _message;
    std::string _key;
};

class BadFlowConfigException : public std::exception {
  public:
    BadFlowConfigException(const std::string& message) : _message(message) {}

    BadFlowConfigException(const BadFlowConfigException&& other) : _message(std::move(other._message)) {}

    const char* what() const noexcept override { return _message.c_str(); }

  private:
    const std::string _message;
};

class InvalidDataException : public std::exception {
  public:
    InvalidDataException(const std::string& message) : _message(message) {}
    const char* what() const noexcept override { return _message.c_str(); }

  private:
    const std::string _message;
};

}; // namespace edgelink
