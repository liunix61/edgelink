cmake_minimum_required(VERSION 3.22)

# You can access the compiler variables set by Vcpkg.


#set(VCPKG_OVERLAY_TRIPLETS ./custom-triplets)

option(EL_WITH_MQTT "Include MQTT support" YES)
#option(EL_WITH_MODBUS "Include ModBus support" YES)

# 按代码尺寸优化
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    string(REPLACE "-O3" "-Os" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-Os" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

set(CMAKE_DEBUG_POSTFIX _d)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置C语言编译器选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

project(EdgeLink)

add_custom_target(git_versioning
  ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/version-config.h.in
                   -D DST=${CMAKE_BINARY_DIR}/version-config.h
                   -P ${CMAKE_SOURCE_DIR}/cmake/GenerateGitVersionHeader.cmake
)

# Workaround for 32-bit cross compiliation
if(TARGET_TRIPLET STREQUAL "arm-linux-dynamic")
    set(CMAKE_SIZEOF_VOID_P 4)
endif()


find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(pegtl REQUIRED)

# 查找Boost库

#set(Boost_DIR ${_VCPKG_INSTALLED_DIR}) # don't set this one. 
set(Boost_NO_SYSTEM_PATHS ON)
#set(Boost_NO_BOOST_CMAKE  ON) # additional set this one
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system url json program_options)
set(Boost_MINIMUM_REQUIRED 1.83.0)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()


# 通过 cmake 引入 ASYNC_MQTT 库
set(_TMP_BUILD_TESTING BUILD_TESTING)
if(BUILD_TESTING)
	set(BUILD_TESTING OFF)
endif()

add_subdirectory(./libs/async_mqtt EXCLUDE_FROM_ALL)
add_subdirectory(./libs/quickjspp/quickjs EXCLUDE_FROM_ALL)
add_subdirectory(./libs/rttr EXCLUDE_FROM_ALL)
add_subdirectory(./libs/meta.hpp EXCLUDE_FROM_ALL)

set(BUILD_TESTING _TMP_BUILD_TESTING)

include_directories(
    ${CMAKE_BINARY_DIR}
    ./include
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/mustache/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/async_mqtt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/rva/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/boost-di/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/croncpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/quickjspp"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/meta.hpp/headers"
)

file(GLOB_RECURSE EL_SOURCES 
    "src/*.c"
    "src/*.cpp"
    "./libs/mustache/src/renderer.cpp"
)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_subdirectory(./abstractions)

############################################## Main App #################################################
# 添加可执行文件
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_executable(${PROJECT_NAME} ${EL_SOURCES})

add_dependencies(${PROJECT_NAME} git_versioning)

add_dependencies(${PROJECT_NAME} EdgeLinkAbstractions)

add_dependencies(${PROJECT_NAME} quickjs)

# target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti), 不要关掉 RTTI

set(EL_DEP_LIBS
    PRIVATE Boost::system
    PRIVATE Boost::json
    PRIVATE spdlog::spdlog
    PRIVATE fmt::fmt
    PRIVATE RTTR::Core
    PRIVATE quickjs
    PRIVATE taocpp::pegtl
    PRIVATE EdgeLinkAbstractions
)

target_link_libraries(${PROJECT_NAME}
    ${EL_DEP_LIBS}
    PRIVATE Boost::program_options
)

add_definitions(-DBOOST_LOG_DYN_LINK)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.hpp)


# 数据文件 -----------------------------------------------------------------------
file(COPY ./resources/nodes/function/prelude.js DESTINATION ${CMAKE_BINARY_DIR}/resources/nodes/function/)


# 测试 ---------------------------------------------------------------------------

# 启用测试

if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 3 REQUIRED)
    if(Catch2_FOUND)
        include_directories(${Catch2_INCLUDE_DIRS})
    endif()

    set(TESTS_EL_SOURCES ${EL_SOURCES})
    list(REMOVE_ITEM TESTS_EL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

    file(GLOB_RECURSE TESTS_SOURCES
        "tests/*.c"
        "tests/*.cpp"
    )

    # 指定测试可执行文件的名称
    add_executable(EdgeLinkTests
        ${TESTS_EL_SOURCES}
        ${TESTS_SOURCES}
    )
    add_dependencies(EdgeLinkTests EdgeLinkAbstractions)

    target_compile_definitions(EdgeLinkTests PRIVATE EL_TEST)

    target_precompile_headers(EdgeLinkTests PRIVATE tests/pch.hpp)

    # 链接cache2和其他项目依赖项
    target_link_libraries(EdgeLinkTests PRIVATE
        ${EL_DEP_LIBS}
        PRIVATE Catch2::Catch2WithMain
    )

    #include(CTest)
    #include(Catch)
    #catch_discover_tests(EdgeLinkTests)

endif()

# 配置文件 ---------------------------------------------------------


# 添加复制操作，将配置文件复制到输出目录
configure_file(./edgelink.ini ${EXECUTABLE_OUTPUT_PATH}/edgelink.ini COPYONLY)
configure_file(./flows.json ${EXECUTABLE_OUTPUT_PATH}/flows.json COPYONLY)


# 插件 ---------------------------------------------------------------------------

# 包含插件子项目

set(PLUGINS_OUTPUT_DIR "${EXECUTABLE_OUTPUT_PATH}/plugins" CACHE INTERNAL "Plugins directory")
file(MAKE_DIRECTORY ${PLUGINS_OUTPUT_DIR})



macro(edgelink_plugin PLUGIN_NAME)
    set(THIS_PLUGIN "EdgeLink.Plugins.${PLUGIN_NAME}")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")
    get_filename_component(CURRENT_CMAKE_SOURCE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PLUGINS_OUTPUT_DIR})

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_library(${THIS_PLUGIN} SHARED ${SOURCES})

    add_dependencies(${THIS_PLUGIN} EdgeLinkAbstractions)
    add_dependencies(${THIS_PLUGIN} git_versioning)
    add_dependencies(${PROJECT_NAME} quickjs)

    target_link_libraries(
        ${THIS_PLUGIN}
        PRIVATE Boost::json
        PRIVATE spdlog::spdlog
        PRIVATE fmt::fmt
        PRIVATE RTTR::Core
        PRIVATE EdgeLinkAbstractions
    )
endmacro()


# 获取所有子目录
file(GLOB PLUGIN_SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/)

# 遍历子目录并添加进来
foreach(CHILD ${PLUGIN_SUBDIRS})
    set(PLUGIN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${CHILD})
    if(IS_DIRECTORY ${PLUGIN_SOURCE_DIR})
        add_subdirectory(${PLUGIN_SOURCE_DIR})

		# 获取子目录的项目列表
		get_directory_property(subdirectory_projects DIRECTORY ${PLUGIN_SOURCE_DIR} DEFINITION THIS_PLUGIN)

		# 输出子目录中的项目名称
		foreach(plugin_project_name IN LISTS subdirectory_projects)
		    message(STATUS "EdgeLink plugin found: ${plugin_project_name}")
            add_dependencies(EdgeLink ${plugin_project_name})
		endforeach()

    endif()
endforeach()





# 打包和安装 ------------------------------------

SET(CPACK_PACKAGE_NAME "edgelink")

set(CPACK_GENERATOR "ZIP")

string(TOLOWER ${CMAKE_SYSTEM_NAME} LOWERCASE_SYSTEM_NAME)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${EL_VERSION}-${CMAKE_SYSTEM_PROCESSOR}-${LOWERCASE_SYSTEM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL OFF)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_STRIP_FILES ON)

include(CPack)
include(GNUInstallDirs)

set(CMAKE_INSTALL_PREFIX "${CPACK_PACKAGE_NAME}" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)

install(TARGETS EdgeLink
    EXPORT elTargets
    RUNTIME DESTINATION
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS EdgeLinkAbstractions
    EXPORT elTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_INSTALL_PREFIX})
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR RES
        UNRESOLVED_DEPENDENCIES_VAR UNRES
        CONFLICTING_DEPENDENCIES_PREFIX CONFLICTING_DEPENDENCIES
        EXECUTABLES $<TARGET_FILE:EdgeLink>
        LIBRARIES
            $<TARGET_FILE:EdgeLinkAbstractions>
            $<TARGET_FILE:EdgeLink.Plugins.Mqtt>
    )
    set(so_include_patterns
        ".*EdgeLink.*"
        ".*boost.*"
        ".*spdlog.*"
        ".*fmt.*"
        ".*rttr.*"
    )

    set(filtered_RES)
    foreach(_file ${RES})
        foreach(pattern ${so_include_patterns})
            string(REGEX MATCH "${pattern}" is_matched "${_file}")
            if(is_matched)
                list(APPEND filtered_RES ${_file})
            endif()
        endforeach()
    endforeach()

    foreach(_file ${filtered_RES})
        message(">> Include shared library: ${_file}")
        file(INSTALL
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
            TYPE SHARED_LIBRARY
            FOLLOW_SYMLINK_CHAIN
            FILES "${_file}"
        )
    endforeach()

    foreach(DEP ${UNRES})
        message(WARNING ">> Unresolved dependency: ${DEP}")
    endforeach()
]])

# 添加要打包的文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_INSTALL_DATADIR}/resources)
