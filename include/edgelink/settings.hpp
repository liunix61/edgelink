#pragma once

namespace edgelink {

/// @brief 程序配置
struct EDGELINK_EXPORT EdgeLinkSettings {
    const std::filesystem::path home_path;           ///< EdgeLink 主目录
    const std::filesystem::path executable_location; ///< EdgeLink 可执行文件所在目录
    const std::string flows_json_path;
};

}; // namespace edgelink
