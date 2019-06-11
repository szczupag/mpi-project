compile:
	mpicc -pthread main.c -o main

run:
	mpirun --use-hwthread-cpus -np ${threads} main

run_many:
	mpirun --oversubscribe -np ${threads} main
