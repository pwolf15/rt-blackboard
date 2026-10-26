cmake_minimum_required(VERSION 3.21)

project(rtbb CXX)

option(RTBB_TSAN "Build with TSAN" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(rtbb INTERFACE)
add_library(rtbb::rtbb ALIAS rtbb)
target_include_directories(rtbb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(rtbb INTERFACE cxx_std_20)

if (PROJECT_IS_TOP_LEVEL)
  include(FetchContent)
  set(INSTALL_GTEST OFF)
  FetchContent_Declare(googletest
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

  add_executable(tests src/tests.cpp)
  target_link_libraries(tests PRIVATE rtbb::rtbb GTest::gtest_main)
  gtest_discover_tests(tests)

  if (RTBB_TSAN)
    target_compile_options(tests PRIVATE -fsanitize=thread)
    target_link_options(tests PRIVATE -fsanitize=thread)
  endif()

  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
