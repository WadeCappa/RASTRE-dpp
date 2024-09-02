mpirun -n 5 ./tools/mpi_find_approximation_set --numberOfRows 32768 --adjacencyListColumnCount 16384 -o m5_sparse_generated.json -k 100 -e 0.000000001 -a 3 -d 2 --distributedEpsilon 0.0372 -T 50 generateInput --rows 32768 --cols 16384 --sparsity 0.5 --seed 21321521532 --generationStrategy 1

