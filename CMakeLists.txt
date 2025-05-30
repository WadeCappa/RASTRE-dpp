cmake_minimum_required(VERSION 3.10)

project(raster_dpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(CLI11 REQUIRED)
find_package(doctest REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenMP REQUIRED)
find_package(spdlog REQUIRED)
find_package(MPI)

add_executable(run_tests src/tests.cpp)
add_executable(single_machine_greedy_find_approximation_set src/single_machine_greedy_find_approximation_set.cpp)
add_executable(single_machine_streaming_find_approximation_set src/single_machine_streaming_find_approximation_set.cpp)
add_executable(get_user_mode_scores src/get_user_mode_scores.cpp)
add_executable(create_user_file src/create_user_file.cpp)

IF(LOG_LEVEL MATCHES debug)
    message("\n--------------------\n\nCOMPILING IN DEBUG MODE\n\n--------------------\n")
    target_compile_options(run_tests PRIVATE -DLOG_DEBUG)
    target_compile_options(single_machine_greedy_find_approximation_set PRIVATE -DLOG_DEBUG)
    target_compile_options(single_machine_streaming_find_approximation_set PRIVATE -DLOG_DEBUG)
    target_compile_options(get_user_mode_scores PRIVATE -DLOG_DEBUG)
    target_compile_options(create_user_file PRIVATE -DLOG_DEBUG)
ENDIF(LOG_LEVEL MATCHES debug)

IF(LOG_LEVEL MATCHES trace)
    message("\n--------------------\n\nCOMPILING IN TRACE MODE\n\n--------------------\n")
    target_compile_options(run_tests PRIVATE -DLOG_TRACE)
    target_compile_options(single_machine_greedy_find_approximation_set PRIVATE -DLOG_TRACE)
    target_compile_options(single_machine_streaming_find_approximation_set PRIVATE -DLOG_TRACE)
    target_compile_options(get_user_mode_scores PRIVATE -DLOG_TRACE)
    target_compile_options(create_user_file PRIVATE -DLOG_TRACE)
ENDIF(LOG_LEVEL MATCHES trace)

target_link_libraries(single_machine_greedy_find_approximation_set spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
target_link_libraries(single_machine_streaming_find_approximation_set spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
target_link_libraries(run_tests spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
target_link_libraries(get_user_mode_scores spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
target_link_libraries(create_user_file spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

target_link_libraries(single_machine_greedy_find_approximation_set CLI11::CLI11)
target_link_libraries(single_machine_streaming_find_approximation_set CLI11::CLI11)
target_link_libraries(run_tests CLI11::CLI11)
target_link_libraries(get_user_mode_scores CLI11::CLI11)
target_link_libraries(create_user_file CLI11::CLI11)

target_link_libraries(run_tests doctest::doctest)

target_link_libraries(run_tests fmt::fmt)
target_link_libraries(single_machine_streaming_find_approximation_set fmt::fmt)
target_link_libraries(single_machine_greedy_find_approximation_set fmt::fmt)
target_link_libraries(get_user_mode_scores fmt::fmt)
target_link_libraries(create_user_file fmt::fmt)

target_link_libraries(single_machine_greedy_find_approximation_set nlohmann_json::nlohmann_json)
target_link_libraries(single_machine_streaming_find_approximation_set nlohmann_json::nlohmann_json)
target_link_libraries(run_tests nlohmann_json::nlohmann_json)
target_link_libraries(get_user_mode_scores nlohmann_json::nlohmann_json)
target_link_libraries(create_user_file nlohmann_json::nlohmann_json)

target_link_libraries(run_tests OpenMP::OpenMP_CXX)
target_link_libraries(single_machine_greedy_find_approximation_set OpenMP::OpenMP_CXX)
target_link_libraries(single_machine_streaming_find_approximation_set OpenMP::OpenMP_CXX)
target_link_libraries(get_user_mode_scores OpenMP::OpenMP_CXX)
target_link_libraries(create_user_file OpenMP::OpenMP_CXX)

if (MPI_FOUND)
    add_executable(mpi_find_approximation_set src/mpi_find_approximation_set.cpp)

    IF(LOG_LEVEL MATCHES debug)
        target_compile_options(mpi_find_approximation_set PRIVATE -DLOG_DEBUG)
    ENDIF(LOG_LEVEL MATCHES debug)

    IF(LOG_LEVEL MATCHES trace)
        target_compile_options(mpi_find_approximation_set PRIVATE -DLOG_TRACE)
    ENDIF(LOG_LEVEL MATCHES trace)

    target_link_libraries(mpi_find_approximation_set spdlog::spdlog)
    target_link_libraries(mpi_find_approximation_set CLI11::CLI11)
    target_link_libraries(mpi_find_approximation_set fmt::fmt)
    target_link_libraries(mpi_find_approximation_set nlohmann_json::nlohmann_json)
    target_link_libraries(mpi_find_approximation_set OpenMP::OpenMP_CXX)

    target_link_libraries(mpi_find_approximation_set MPI::MPI_CXX)

    target_link_libraries(single_machine_streaming_find_approximation_set MPI::MPI_CXX)
endif()
