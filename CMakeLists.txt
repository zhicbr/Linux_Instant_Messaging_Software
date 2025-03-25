cmake_minimum_required(VERSION 3.16)
project(project1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(ChatServer)
add_subdirectory(ChatClient)
