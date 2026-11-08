cmake_minimum_required(VERSION 3.22)

if(EL_WITH_PROTOCOL_SOURCE)

    edgelink_plugin("ProtocolSource")

    # 共享内存事件队列访问（webhmi /dev/shm/webhmi_events）
    target_link_libraries(${THIS_PLUGIN}
        PRIVATE
    )

    # C3 展示节点：nng WS pub 广播（HMI 订阅）
    set(NANOSDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/nanosdk)
    set(NANOSDK_BUILD ${NANOSDK_DIR}/build)
    set(NANOSDK_LIB ${NANOSDK_BUILD}/libnng.a)
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
    target_include_directories(${THIS_PLUGIN} PRIVATE ${NANOSDK_DIR}/include)
    target_link_libraries(${THIS_PLUGIN} PRIVATE ${NANOSDK_LIB} pthread)

    target_precompile_headers(${THIS_PLUGIN} PRIVATE src/pch.hpp)

    # 测试：shm_event_queue 跨进程收发 + 无锁并发（独立可执行）
    enable_testing()
    add_executable(test_shm_event_queue test/test_shm_event_queue.cpp src/shm_event_queue.cpp)
    target_include_directories(test_shm_event_queue PRIVATE src)
    target_link_libraries(test_shm_event_queue PRIVATE pthread)
    add_test(NAME test_shm_event_queue COMMAND test_shm_event_queue)

    # 测试：shm_command_queue 跨进程收发 + 无锁并发
    add_executable(test_shm_command_queue test/test_shm_command_queue.cpp src/shm_command_queue.cpp)
    target_include_directories(test_shm_command_queue PRIVATE src)
    target_link_libraries(test_shm_command_queue PRIVATE pthread)
    add_test(NAME test_shm_command_queue COMMAND test_shm_command_queue)

endif()
