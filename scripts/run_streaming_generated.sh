mpirun -n 2 ./tools/streaming_find_approximation_set --output test2.json --numberOfRows 1024 -k 10 -e 0.000000001 -a 4 -d 2 --distributedEpsilon 0.0372 -T 50 generateInput --rows 1024 --cols 1024 --seed 21321521532