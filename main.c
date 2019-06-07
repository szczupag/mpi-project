#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define NEW_TASK 100;

enum type {GLOWA, OGON, TULOW, ZLECENIEDOWCA};

const char* typeNames[] = {"GLOWA", "OGON", "TULOW", "ZLECENIEDOWCA"};

int rank, size, lamportTimestamp = 0;

void *workerMainFunc(void *ptr) {
    MPI_Status status;
//    int size;
//
//    MPI_Comm_size(MPI_COMM_WORLD, &size);
//
//    int x = 1;
//    int data = 0;
//    while (x) {
//        x--;
//        printf("\t%d czeka\n", rank);
//
//        if ((rank == 0) && (x == 0)) {
//            printf("0 Będę spał\n");
//            sleep(10);
//            printf("0 kończy spanie\n");
//        }
//
//        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
//
//        printf("%d dostal %d od %d\n", rank, data, status.MPI_SOURCE);
//
//        data++;
//        if (data >= size) {
//            printf("[%d] Pierwszy koniec dla %d\n", rank, data);
//        } else if (rank == size - 1) {
//            printf("%d wysyla %d do 0\n", rank, data);
//            /* MPI_Ssend teoretycznie powinien się blokować aż do matching receive
//                (tak twierdzi kilka wyguglanych stron)
//            */
//
//            MPI_Ssend(&data, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
//            printf("%d wyslanie %d do 0 się powiodło \n", rank, data);
//        } else {
//            printf("%d wysyla %d do %d\n", rank, data, rank + 1);
//            MPI_Ssend(&data, 1, MPI_INT, rank + 1, 1, MPI_COMM_WORLD);
//        }
//    }

}

void totalBroadcast(enum type sendTo, int * task, int taskSize) {
    for (int processId = sendTo + 1; processId < size; processId += 3) {
        MPI_Send(task, 1, MPI_INT, processId, *task, MPI_COMM_WORLD);
    }
}

enum type assignProfession(int processRank) {
    if (processRank == 0) {
        return ZLECENIEDOWCA;
    } else {
        switch (processRank % 3) {
            case 0:
                return OGON;
            case 1:
                return GLOWA;
            case 2:
                return TULOW;
        }
    }
}

void initThreadSystem(int threadSystem, int * processRank, int * size) {
    if (threadSystem != MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
        MPI_Finalize();
        exit(-1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, processRank);
    MPI_Comm_size(MPI_COMM_WORLD, size);
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);

    enum type profession = assignProfession(rank);
    printf("Proces %d: %s", rank, typeNames[profession]);

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerMainFunc, 0);

    int data = 0;

    sleep(rank + 1);

    /*   if (rank==size-1)
       MPI_Ssend( &data, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
       else */
    if (rank == 0) {
        MPI_Ssend(&data, 1, MPI_INT, rank + 1, 1, MPI_COMM_WORLD);
    }

    printf("--> [%d] Poszlo! %d \n", rank, data);

    pthread_join(workerThread, NULL);

    printf("    %d koniec!\n", rank);
    MPI_Finalize();
}
