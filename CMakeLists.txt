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

# Fetch funkyactors via FetchContent
include(FetchContent)
message(STATUS "Downloading funkyactors via FetchContent")
FetchContent_Declare(
  funkyactors
  GIT_REPOSITORY https://github.com/mahush/funkyactors.git
  GIT_TAG main
)
FetchContent_MakeAvailable(funkyactors)

# Fetch funkypipes via FetchContent
message(STATUS "Downloading funkypipes via FetchContent")
FetchContent_Declare(
  funkypipes
  GIT_REPOSITORY https://github.com/mahush/funkypipes.git
  GIT_TAG main
)
FetchContent_MakeAvailable(funkypipes)

# Create an interface library for funkypipes
add_library(funkypipes INTERFACE)
target_include_directories(funkypipes INTERFACE ${funkypipes_SOURCE_DIR}/include)

# Main executable
add_executable(snake_actors
  src/main.cpp
  semantic_artifacts/src/classic/classic_arena_authority.cpp
  semantic_artifacts/src/classic/classic_game_lifecycle_authority.cpp
  semantic_artifacts/src/classic/scoring_policy.cpp
  semantic_artifacts/src/classic/classic_game_domain_application.cpp
  semantic_artifacts/src/classic/direction_command_filter.cpp
  semantic_artifacts/src/common/game_logic.cpp
  semantic_artifacts/src/common/snake_predicates.cpp
  src/game_manager_actor.cpp
  src/game_engine_actor.cpp
  src/input_actor.cpp
  src/stdin_reader.cpp
  src/renderer_actor.cpp
  semantic_artifacts/src/common/snake_model.cpp
  src/utility.cpp
  src/logger.cpp
)

target_include_directories(snake_actors PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/semantic_artifacts/include
)

target_link_libraries(snake_actors PRIVATE funkyactors funkypipes)

target_compile_options(snake_actors PRIVATE
  -Wall -Wextra -Wpedantic -Werror
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
  add_executable(test_snake
    tests/test_actors.cpp
    tests/test_generic_lens.cpp
    tests/test_classic_arena_authority.cpp
    tests/test_direction_command_filter.cpp
    tests/test_snake_model.cpp
    tests/test_classic_game_lifecycle_authority.cpp
    tests/test_scoring_policy.cpp
    tests/test_difficulty_policy.cpp
    tests/test_orbital_latency_policy.cpp
    tests/test_orbital_pursuit_policy.cpp
    tests/test_orbital_order_authority.cpp
    semantic_artifacts/src/orbital/order_authority.cpp
    tests/test_orbital_arena_authority.cpp
    semantic_artifacts/src/orbital/arena_authority.cpp
    tests/test_orbital_scoring_policy.cpp
    semantic_artifacts/src/orbital/scoring_policy.cpp
    semantic_artifacts/src/orbital/pursuit_policy.cpp
    tests/test_classic_game_domain_application.cpp
    semantic_artifacts/src/classic/classic_arena_authority.cpp
    semantic_artifacts/src/classic/classic_game_lifecycle_authority.cpp
    semantic_artifacts/src/classic/scoring_policy.cpp
    semantic_artifacts/src/classic/classic_game_domain_application.cpp
    semantic_artifacts/src/classic/direction_command_filter.cpp
    semantic_artifacts/src/common/game_logic.cpp
    semantic_artifacts/src/common/snake_predicates.cpp
    src/game_manager_actor.cpp
    src/game_engine_actor.cpp
    src/input_actor.cpp
    src/stdin_reader.cpp
    src/renderer_actor.cpp
    semantic_artifacts/src/common/snake_model.cpp
    src/utility.cpp
    src/logger.cpp
  )

  target_link_libraries(test_snake PRIVATE gtest_main gmock_main funkyactors funkypipes)
  target_include_directories(test_snake PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/semantic_artifacts/include
    ${PROJECT_SOURCE_DIR}/tests
  )
  target_compile_options(test_snake PRIVATE -Wall -Wextra -Wpedantic -Werror)

  include(GoogleTest)
  gtest_discover_tests(test_snake)
endif()

# Install target (optional)
install(DIRECTORY include/ semantic_artifacts/include/ DESTINATION include)
