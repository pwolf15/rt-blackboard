cmake_minimum_required(VERSION 3.20)

project(rtbb CXX)

option(RTBB_TSAN "Build with TSAN" ON)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
set(INSTALL_GTEST OFF)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_executable(tests src/tests.cpp)

if (RTBB_TSAN)
  target_compile_options(tests PRIVATE -fsanitize=thread)
  target_link_options(tests PRIVATE -fsanitize=thread)
endif()

gtest_discover_tests(tests)
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tests PRIVATE GTest::gmock_main)
