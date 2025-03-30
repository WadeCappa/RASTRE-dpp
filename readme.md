# RAnd-greedi STREming for approximating Determinantal Point Process

## Install Instructions
Set up virtual enviroment
```
python3 -m venv venv`
source venv/bin/activate
pip install conan
```

Set up conan
```
conan profile detect --force
conan install . --output-folder=build --build=missing
```

Build
```
chmod +x build.sh
./build.sh
```

### Install FAQ 
- If the `build.sh` script succeeds but the MPI tool doesn't compile, double check if you have an mpi module loaded in your local enviroment. The `build.sh` script assumes that your enviroment has access to openmpi-x86_64, but if this is not the case CMAKE will skip the compilation of the mpi tool. Any MPI module should work, so if you're not using openmpi just make sure that you load any alternative module in its place.

## Usage


## Debugging
To run the mpi tool with gdb you can use the following command;
```
mpirun -n 3 <terminal emulator> -hold -e gdb -ex run --args ./tools/mpi_find_approximation_set <arg1> <arg2> ...
```
