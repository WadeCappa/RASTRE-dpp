#!/usr/bin/env bash

LOG_LEVEL="${1:-$Info}"
module load mpi/openmpi-x86_64
echo "using log level ${LOG_LEVEL}"
cmake . -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DLOG_LEVEL=${LOG_LEVEL}
cmake --build .
