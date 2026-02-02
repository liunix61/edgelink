cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THE_LIB "EdgeLinkAbstractions")

file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(pegtl REQUIRED)

get_filename_component(CURRENT_CMAKE_SOURCE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

#add_subdirectory(${CURRENT_CMAKE_SOURCE_DIR}/libs/async_mqtt)

add_compile_options(-fvisibility=hidden)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(${THE_LIB} SHARED ${SOURCES})

# 包含Boost头文件
target_include_directories(${THE_LIB}
    PRIVATE ${Boost_INCLUDE_DIRS}
    PRIVATE ${../include}
)

target_link_libraries(
    ${THE_LIB}
    PRIVATE Boost::json
    PRIVATE fmt::fmt
    PRIVATE spdlog::spdlog
    PRIVATE RTTR::Core
    PRIVATE taocpp::pegtl
)

target_precompile_headers(${THE_LIB} PRIVATE src/pch.hpp)



#add_custom_command(
#    TARGET ${THE_LIB}
#    POST_BUILD
#    COMMAND cp $<TARGET_FILE:${THE_LIB}> ${PLUGINS_OUTPUT_DIR}/..
#)


#file(COPY "${CMAKE_BINARY_DIR}/${PLUGIN_MQTT}.so" DESTINATION "${PLUGINS_OUTPUT_DIR}")