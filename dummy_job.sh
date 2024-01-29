#!/bin/bash

#SBATCH -A m1641
#SBATCH -C cpu
#SBATCH -t 00:01:00
#SBATCH -q debug 
#SBATCH -N 2
#SBATCH --ntasks-per-node=1
#SBATCH -J dummy
#SBATCH -o /global/homes/r/reetb/dummy.o
#SBATCH -e /global/homes/r/reetb/dummy.e

#OpenMP settings:
export OMP_NUM_THREADS=128
export OMP_PLACES=threads
export OMP_PROC_BIND=spread

srun -n 2 ./tools/mpi_find_approximation_set -i /global/cfs/cdirs/m1641/DPP_MAP/MovieLens/MovieLens_matrix_small.txt -o /global/homes/r/reetb/results/dpp/test/m4_lazyfast_lazy.json -k 10 -e 0.000000001 --numberOfRows 409 -a 3