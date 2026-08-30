cmake_minimum_required(VERSION 3.16)
# 使用 clang 作为编译器
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(CppTest LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # 可执行文件 (.exe / 无后缀)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # 静态库 (.a)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # 动态库 (.so)

# 头文件搜索路径（项目根，让 #include "network/..." 生效）
set(PROJECT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# ============ 网络协议库（静态库，供可执行程序和测试共同链接） ============
add_library(network STATIC
    network/l3/ipv6/ipv6.cpp
)

target_include_directories(network PUBLIC
    ${PROJECT_INCLUDE_DIRS}
)

# ============ 演示程序 ============
add_executable(util)

target_sources(util PRIVATE
    main.cpp
    second.cpp
)

target_link_libraries(util PRIVATE network)

# ============ 单元测试（doctest：单头文件，零依赖） ============
include(CTest)   # 提供 BUILD_TESTING 选项，并集成 ctest

if(BUILD_TESTING)
    # doctest 头文件位置（third_party/doctest.h）
    set(DOCTEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

    # 新增测试的方法：在 test/ 目录下建 test_xxx.cpp，然后往这个列表加一行
    set(TEST_SOURCES
        test/test_ipv6.cpp
        test/test_mac_linklocal.cpp
    )

    foreach(src ${TEST_SOURCES})
        get_filename_component(test_name ${src} NAME_WE)
        add_executable(${test_name} ${src})
        target_include_directories(${test_name} PRIVATE
            ${PROJECT_INCLUDE_DIRS}
            ${DOCTEST_INCLUDE_DIR}
        )
        target_link_libraries(${test_name} PRIVATE network)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
