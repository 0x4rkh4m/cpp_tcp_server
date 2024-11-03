cmake_minimum_required(VERSION 3.29)
project(cpp_tcp_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/main.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Create a library from the server source files
add_library(server_lib src/server.cpp)

# Add the server executable
add_executable(cpp_tcp_server src/main.cpp)
target_link_libraries(cpp_tcp_server server_lib)

# Add the test executable
add_executable(server_test tests/server_test.cpp)
target_link_libraries(server_test gtest_main server_lib)

include(GoogleTest)
gtest_discover_tests(server_test)
