cmake_minimum_required(VERSION 3.16)
project(CAN_UDS_Tool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 代码覆盖率选项（仅GCC/Clang，Debug模式下使用）
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Code coverage: ENABLED")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

message(STATUS "Using Qt${QT_VERSION_MAJOR}")

# 核心协议栈库（纯C++，无Qt依赖）
add_library(core_protocol STATIC
    src/core/can/can_frame.h
    src/core/can/can_filter.h
    src/core/can/can_filter.cpp
    src/core/can/can_stats.h
    src/core/can/can_stats.cpp
    src/core/dbc/dbc_parser.h
    src/core/dbc/dbc_parser.cpp
    src/core/dbc/dbc_signal.h
    src/core/dbc/dbc_message.h
    src/core/isotp/isotp_client.h
    src/core/isotp/isotp_client.cpp
    src/core/uds/uds_client.h
    src/core/uds/uds_client.cpp
    src/core/uds/uds_types.h
)

target_include_directories(core_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

# 硬件抽象层库
add_library(hal_layer STATIC
    src/hal/can_adapter_base.h
    src/hal/can_adapter_base.cpp
    src/hal/virtual_can_adapter.h
    src/hal/virtual_can_adapter.cpp
)

target_include_directories(hal_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

target_link_libraries(hal_layer PUBLIC core_protocol)

# Linux下添加SocketCAN支持
if(UNIX AND NOT APPLE)
    target_sources(hal_layer PRIVATE
        src/hal/socket_can_adapter.h
        src/hal/socket_can_adapter.cpp
    )
    target_compile_definitions(hal_layer PRIVATE HAS_SOCKETCAN)
endif()

# 业务层库（Qt Core依赖）
add_library(business_layer STATIC
    src/business/can_manager.h
    src/business/can_manager.cpp
    src/business/dbc_manager.h
    src/business/dbc_manager.cpp
    src/business/diag_manager.h
    src/business/diag_manager.cpp
    src/business/log_manager.h
    src/business/log_manager.cpp
    src/business/signal_manager.h
    src/business/signal_manager.cpp
)

target_include_directories(business_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/business
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
)

target_link_libraries(business_layer PUBLIC
    core_protocol
    hal_layer
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

# GUI可执行文件
add_executable(can_uds_tool
    src/gui/main.cpp
    src/gui/main_window.h
    src/gui/main_window.cpp
    src/gui/panels/trace_panel.h
    src/gui/panels/trace_panel.cpp
    src/gui/panels/signal_panel.h
    src/gui/panels/signal_panel.cpp
    src/gui/panels/diag_panel.h
    src/gui/panels/diag_panel.cpp
    src/gui/panels/config_panel.h
    src/gui/panels/config_panel.cpp
    src/gui/panels/replay_panel.h
    src/gui/panels/replay_panel.cpp
    src/gui/panels/send_panel.h
    src/gui/panels/send_panel.cpp
    src/gui/panels/signal_value_panel.h
    src/gui/panels/signal_value_panel.cpp
    src/gui/panels/stats_panel.h
    src/gui/panels/stats_panel.cpp
    resources/resources.qrc
)

target_link_libraries(can_uds_tool PRIVATE
    business_layer
    Qt${QT_VERSION_MAJOR}::Widgets
)

# 单元测试
enable_testing()
add_subdirectory(tests)
