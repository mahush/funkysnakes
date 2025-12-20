cmake_minimum_required(VERSION 3.12)

# Enable generation of compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(snake LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")

# Fetch standalone Asio via FetchContent
message(STATUS "Downloading standalone Asio via FetchContent")
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-30-2
)
FetchContent_MakeAvailable(asio)

# Create an interface library for asio
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

# Link with pthread on Unix systems
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(asio INTERFACE Threads::Threads)
endif()

# Main executable
add_executable(snake_actors
  src/main.cpp
  src/calculator_actor.cpp
  src/result_actor.cpp
)

target_include_directories(snake_actors PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(snake_actors PRIVATE asio)

target_compile_options(snake_actors PRIVATE
  -Wall -Wextra -Wpedantic
  $<$<CONFIG:Debug>:-g -O0>
  $<$<CONFIG:Release>:-O3>
)

# Optional: Build tests (ON by default for development)
option(SNAKE_BUILD_TESTS "Build snake unit tests" ON)
if(SNAKE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  )
  FetchContent_MakeAvailable(googletest)

  # Suppress warnings for gtest and gmock
  if(TARGET gtest)
    target_compile_options(gtest PRIVATE -w)
  endif()
  if(TARGET gmock)
    target_compile_options(gmock PRIVATE -w)
  endif()

  enable_testing()
  add_executable(test_snake tests/test_actors.cpp src/calculator_actor.cpp src/result_actor.cpp)

  target_link_libraries(test_snake PRIVATE gtest_main gmock_main asio)
  target_include_directories(test_snake PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests
  )
  target_compile_options(test_snake PRIVATE -Wall -Wextra -Wpedantic)

  include(GoogleTest)
  gtest_discover_tests(test_snake)
endif()

# Install target (optional)
install(DIRECTORY include/ DESTINATION include)
