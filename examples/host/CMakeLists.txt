# =============================================================================
# AT25040B driver: host example (any PC, no hardware)
# =============================================================================
# The driver against a simulated AT25040B (the protocol model the host tests
# use, tests/mock_at25040b.hpp) through the host port in this folder.
#
#   cmake -S examples/host -B build-example
#   cmake --build build-example && ./build-example/at25040b_host_example
#
# The host tests also build this folder and run the example (CTest
# at25040b_host_example), so it always compiles against the current headers.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(at25040b_host_example LANGUAGES CXX)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. at25040b)
endif()

# ISO C++20, also when this folder is built on its own (handbook KB-CPP20-001).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(at25040b_host_example main.cpp)
target_link_libraries(at25040b_host_example PRIVATE at25040b::at25040b)
target_include_directories(at25040b_host_example PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../../tests)
target_compile_features(at25040b_host_example PRIVATE cxx_std_20)
target_compile_options(at25040b_host_example PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic
  -pedantic-errors)
