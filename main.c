#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define NEW_TASK 100

enum type {OGON, GLOWA, TULOW, ZLECENIEDOWCA};

const char* typeNames[] = {"OGON", "GLOWA", "TULOW", "ZLECENIEDOWCA"};

int rank, size, lamportTimestamp = 0;

int* ogonTeamQueue, tulowTeamQueue, glowaTeamQueue;

void messangerTulow(){
    MPI_Status status;
    int data = 0;
    MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == NEW_TASK) {

    }

    printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
}

void *workerMainFunc(void *ptr) {
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);
}

void totalBroadcast(enum type sendTo, int task) {
    for (int processId = sendTo + 1; processId < size; processId += 3) {
        MPI_Send(&task, 1, MPI_INT, processId, task, MPI_COMM_WORLD);
    }
}


enum type assignProfession(int processRank) {
    if (processRank == 0) {
        return ZLECENIEDOWCA;
    } else {
        switch (processRank % 3) {
            case 0:
                return TULOW;
            case 1:
                return OGON;
            case 2:
                return GLOWA;
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

void zleceniodawca() {
    sleep(2);
    totalBroadcast(TULOW, NEW_TASK);
}


void tulow() {
    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerMainFunc, 0);
    messangerTulow();
    pthread_join(workerThread, NULL);
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);

    enum type profession = assignProfession(rank);
    printf("[PROCES %d - MAIN] type %s \n", rank, typeNames[profession]);

    switch (profession) {
        case ZLECENIEDOWCA:
            zleceniodawca();
            break;

        case TULOW:
            tulow();
            break;

        case GLOWA:
            break;

        case OGON:
            break;

        default:
            break;
    }

    printf("[PROCES %d - MAIN] koniec \n", rank);
    MPI_Finalize();
}
