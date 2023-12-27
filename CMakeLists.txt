cmake_minimum_required(VERSION 3.10)

project(preprocess_data)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

find_package(CLI11 REQUIRED)

add_executable(preprocess_data src/preprocess_data.cpp)
target_link_libraries(preprocess_data CLI11::CLI11)