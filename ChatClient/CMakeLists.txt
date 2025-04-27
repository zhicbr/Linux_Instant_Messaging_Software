cmake_minimum_required(VERSION 3.16)
project(ChatClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用自动 MOC 和 RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt6 模块，仅保留 QML 相关模块
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Quick Qml Widgets)

# 定义源文件
set(SOURCES
    src/main.cpp
    src/chatwindow.cpp
    src/chatwindow.h
    ../Common/messageprotocol.cpp
    ../Common/messageprotocol.h
    ../Common/logger.cpp
    ../Common/logger.h
    ../Common/config.h
    ../Common/utils.h
)

# 添加 QML 资源
qt_add_resources(RESOURCES resources.qrc)

# 创建可执行文件
add_executable(ChatClient
    ${SOURCES}
    ${RESOURCES}
)

# 添加 Common 目录到包含路径
target_include_directories(ChatClient PRIVATE ../Common)

# 链接 Qt 库
target_link_libraries(ChatClient PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Quick
    Qt6::Qml
    Qt6::Widgets
)

# 设置目标属性
set_target_properties(ChatClient PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)