cmake_minimum_required(VERSION 3.22)

if(EL_WITH_MQTT)

    edgelink_plugin("Mqtt")

    # 去 Boost：MQTT 客户端改用 EMQX NanoSDK（nng 纯 C，零 Boost）
    set(NANOSDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/nanosdk)
    set(NANOSDK_BUILD ${NANOSDK_DIR}/build)
    set(NANOSDK_LIB ${NANOSDK_BUILD}/libnng.a)

    # NanoSDK 是独立 CMake project(nng)（纯 C C99），不能直接 add_subdirectory（project 冲突）。
    # 用 ExternalProject 在构建时自动编译 libnng.a（submodule 干净源码无预编译产物）。
    # 已存在则跳过编译（增量构建友好）。
    if(NOT EXISTS ${NANOSDK_LIB})
        include(ExternalProject)
        ExternalProject_Add(nanosdk_build
            SOURCE_DIR ${NANOSDK_DIR}
            BINARY_DIR ${NANOSDK_BUILD}
            INSTALL_DIR ${NANOSDK_BUILD}
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=Release
                -DNNG_ENABLE_QUIC=OFF
                -DBUILD_SHARED_LIBS=OFF
                -DCMAKE_C_FLAGS=-fPIC
            BUILD_BYPRODUCTS ${NANOSDK_LIB}
            BUILD_COMMAND cmake --build . --target nng
            INSTALL_COMMAND ""
        )
        add_dependencies(${THIS_PLUGIN} nanosdk_build)
    endif()

    target_include_directories(${THIS_PLUGIN}
        PRIVATE ${NANOSDK_DIR}/include
    )

    # 链接 NanoSDK 静态库（libnng.a）——自动构建或已存在
    target_link_libraries(${THIS_PLUGIN}
        PRIVATE ${NANOSDK_LIB}
        PRIVATE pthread
    )

    target_precompile_headers(${THIS_PLUGIN} PRIVATE src/pch.hpp)

endif()
