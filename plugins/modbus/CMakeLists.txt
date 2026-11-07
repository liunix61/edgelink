cmake_minimum_required(VERSION 3.22)

if(EL_WITH_MODBUS)

    edgelink_plugin("Modbus")

    find_library(LIBMODBUS NAMES modbus REQUIRED)

    # 去 Boost：async_mqtt（内部 boost::asio）已移除——modbus 源码零 async_mqtt 引用，清理死引用
    target_link_libraries(${THIS_PLUGIN}
        PRIVATE ${LIBMODBUS}
    )

    target_precompile_headers(${THIS_PLUGIN} PRIVATE src/pch.hpp)

endif()