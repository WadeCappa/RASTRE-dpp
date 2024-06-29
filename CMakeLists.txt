cmake_minimum_required(VERSION 3.10)

project(run_tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

find_package(CLI11 REQUIRED)
find_package(doctest REQUIRED)
find_package(fmt REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenMP REQUIRED)
find_package(MPI)

add_executable(run_tests src/tests.cpp)
add_executable(find_approximation_set src/find_approximation_set.cpp)

target_link_libraries(find_approximation_set CLI11::CLI11)
target_link_libraries(run_tests CLI11::CLI11)

target_link_libraries(run_tests doctest::doctest)

target_link_libraries(run_tests fmt::fmt)
target_link_libraries(find_approximation_set fmt::fmt)

target_link_libraries(run_tests Eigen3::Eigen)
target_link_libraries(find_approximation_set Eigen3::Eigen)

target_link_libraries(find_approximation_set nlohmann_json::nlohmann_json)
target_link_libraries(run_tests nlohmann_json::nlohmann_json)

target_link_libraries(run_tests OpenMP::OpenMP_CXX)
target_link_libraries(find_approximation_set OpenMP::OpenMP_CXX)

if (MPI_FOUND)
    add_executable(mpi_find_approximation_set src/mpi_find_approximation_set.cpp)
    target_link_libraries(mpi_find_approximation_set CLI11::CLI11)
    target_link_libraries(mpi_find_approximation_set fmt::fmt)
    target_link_libraries(mpi_find_approximation_set Eigen3::Eigen)
    target_link_libraries(mpi_find_approximation_set nlohmann_json::nlohmann_json)
    target_link_libraries(mpi_find_approximation_set OpenMP::OpenMP_CXX)

    target_link_libraries(mpi_find_approximation_set MPI::MPI_CXX)
endif()
