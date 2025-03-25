cmake_minimum_required(VERSION 3.16)
project(ChatServer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

add_executable(ChatServer
    src/main.cpp
    src/server.cpp
    ../Common/messageprotocol.cpp
)

target_include_directories(ChatServer PRIVATE ../Common)
target_link_libraries(ChatServer PRIVATE Qt6::Core Qt6::Network)
