mpirun -n 5 ./tools/mpi_find_approximation_set --numberOfRows 100000 --adjacencyListColumnCount 1000 -o m5_sparse_generated -k 100 -e 0.000000001 -a 3 -d 3 --distributedEpsilon 0.0372 -T 50 generateInput --rows 100000 --cols 1000 --sparsity 0.99 --seed 21321521532

