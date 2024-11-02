cmake_minimum_required(VERSION 3.29)
project(cpp_tcp_server)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(cpp_tcp_server src/main.cpp src/server.cpp)
