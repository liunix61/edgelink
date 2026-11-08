cmake_minimum_required(VERSION 3.22)

if(EL_WITH_PROTOCOL_SOURCE)

    edgelink_plugin("ProtocolSource")

    # 共享内存事件队列访问（webhmi /dev/shm/webhmi_events）
    target_link_libraries(${THIS_PLUGIN}
        PRIVATE
    )

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
