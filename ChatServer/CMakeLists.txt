cmake_minimum_required(VERSION 3.16)
project(ChatServer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Network Sql REQUIRED)

add_executable(ChatServer
    src/main.cpp
    src/server.cpp
    src/server.h
    src/processmanager.cpp
    src/processmanager.h
    src/sharedmemory.cpp
    src/sharedmemory.h
    src/messagequeue.cpp
    src/messagequeue.h
    src/semaphore.cpp
    src/semaphore.h
    src/filelock.cpp
    src/filelock.h
    src/readwritelock.cpp
    src/readwritelock.h
    src/threadpool.cpp
    src/threadpool.h
    src/threadmessagequeue.cpp
    src/threadmessagequeue.h
    ../Common/messageprotocol.cpp
    ../Common/messageprotocol.h
    ../Common/logger.cpp
    ../Common/logger.h
    ../Common/config.h
    ../Common/utils.h
)

target_include_directories(ChatServer PRIVATE ../Common)

# Link Qt libraries and also pthread (for std::thread) and rt (for POSIX semaphores)
target_link_libraries(ChatServer PRIVATE Qt6::Core Qt6::Network Qt6::Sql pthread rt)
