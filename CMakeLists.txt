cmake_minimum_required(VERSION 3.10)

project(preprocess_data)
project(run_tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

find_package(CLI11 REQUIRED)
find_package(doctest REQUIRED)

add_executable(preprocess_data src/preprocess_data.cpp)
add_executable(run_tests src/tests.cpp)

target_link_libraries(preprocess_data CLI11::CLI11)
target_link_libraries(run_tests doctest::doctest)