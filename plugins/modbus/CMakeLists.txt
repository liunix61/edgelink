cmake_minimum_required(VERSION 3.22)

if(EL_WITH_MODBUS)

    edgelink_plugin("Modbus")

    find_library(LIBMODBUS NAMES modbus REQUIRED)

    target_include_directories(${THIS_PLUGIN}
        PRIVATE ./libs/async_mqtt/include
    )

    target_link_libraries(${THIS_PLUGIN}
        PRIVATE async_mqtt_iface
        PRIVATE ${LIBMODBUS}
    )

    target_precompile_headers(${THIS_PLUGIN} PRIVATE src/pch.hpp)

endif()