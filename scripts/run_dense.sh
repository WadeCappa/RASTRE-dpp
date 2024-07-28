mpirun -n 5 ./tools/mpi_find_approximation_set --numberOfRows 1633 -o m5_movielens.json -k 100 -e 0.000000001 -a 3 -d 2 --distributedEpsilon 0.0372 -T 50 loadInput -i MovieLens_matrix_1600.txt
