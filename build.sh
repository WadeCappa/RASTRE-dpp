module load mpi/openmpi-x86_64
cmake . -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
./tools/run_tests
