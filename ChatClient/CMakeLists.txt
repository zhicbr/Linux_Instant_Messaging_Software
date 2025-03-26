cmake_minimum_required(VERSION 3.16)
project(ChatClient LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 添加 UI 文件搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

find_package(Qt6 COMPONENTS Core Gui Network Widgets REQUIRED)

add_executable(ChatClient
    src/main.cpp
    src/chatwindow.cpp
    src/chatwindow.h
    ui/chatwindow.ui
    ../Common/messageprotocol.cpp
    ../Common/messageprotocol.h
    ../Common/config.h
    ../Common/utils.h
)

target_include_directories(ChatClient PRIVATE ../Common)
target_link_libraries(ChatClient PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets)

set_target_properties(ChatClient PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
