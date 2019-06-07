compile:
	mpicc -pthread main.c -o main

run:
	mpirun --use-hwthread-cpus -np ${threads} main
