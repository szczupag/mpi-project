#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define NEW_TASK 100
#define READY_FOR_NEW_TASK 200

pthread_mutex_t lamportLock, lamportArrayLock;

enum type {OGON, GLOWA, TULOW, ZLECENIEDOWCA};

const char* typeNames[] = {"OGON", "GLOWA", "TULOW", "ZLECENIEDOWCA"};

int rank, size, lamportTS = 0, *otherTS;

typedef struct QueueElement {
    int pID;
    int pLamport;
    struct QueueElement * next;
} QueueElementType;

QueueElementType *glowaTeamQueue, *ogonTeamQueue, *tulowTeamQueue;

void insertAfter(QueueElementType *current, int processID, int processLamport){
    QueueElementType *tmp = current->next;
    current->next=(QueueElementType *)malloc(sizeof(QueueElementType));
    current->next->pID=processID;
    current->next->pLamport=processLamport;
    current->next->next=tmp;
}

void insertToQueue(QueueElementType **head, int processID, int processLamport) {
    if(*head==NULL) {
        *head = (QueueElementType *)malloc(sizeof(QueueElementType));
        (*head)->pID = processID;
        (*head)->pLamport = processLamport;
        (*head)->next = NULL;
    } else if((*head)->next==NULL){
        if((*head)->pLamport< processLamport || ((*head)->pLamport== processLamport && (*head)->pID<processID) ){
            insertAfter(*head,processID,processLamport);
        } else {
            QueueElementType *new = (QueueElementType *)malloc(sizeof(QueueElementType));
            new->pLamport= processLamport;
            new->pID = processID;
            new->next = *head;
            (*head) = new;
        }
    } else {
        QueueElementType *current=*head;
        while (current->next != NULL && ( current->next->pLamport < processLamport || ( current->next->pLamport == processLamport && current->next->pID < processID) ) ) {
            current = current->next;
        }
        insertAfter(current, processID, processLamport);
    }
}


void showQueue(QueueElementType *head, int type) {
    printf("[PROCES %d - QUEUE] %s: ", rank, typeNames[type]);
    if(head==NULL) printf("Queue is empty");
    else
    {
        QueueElementType *current=head;
        do {
            printf("{ procesID: %d, lamportTS: %d }, ", current->pID, current->pLamport);
            current = current->next;
        }while (current != NULL);
    }
    printf("\n");
}

void lamportIncreaseAfterRecv(int senderRank){
    pthread_mutex_lock(&lamportLock);
    if(senderRank>lamportTS){
        lamportTS = senderRank + 1;
    } else {
        lamportTS += 1;
    }
    pthread_mutex_unlock(&lamportLock);
}

void lamportIncreaseBeforeSend(){
    lamportTS += 1;
}

void professionalistsBroadcast(int task){
    pthread_mutex_lock(&lamportLock);
    lamportIncreaseBeforeSend();
    for (int processId = 1; processId < size; processId += 1){
        MPI_Send(&lamportTS, 1, MPI_INT, processId, task, MPI_COMM_WORLD);
    }
    pthread_mutex_unlock(&lamportLock);
}

void singleProfessionBroadcast(enum type sendTo, int task) {
    lamportIncreaseBeforeSend();
    for (int processId = sendTo + 1; processId < size; processId += 3) {
        MPI_Send(&lamportTS, 1, MPI_INT, processId, task, MPI_COMM_WORLD);
    }
}

enum type getProfession(int processRank) {
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

void messanger(){

    for(int i = 0; i<size-1; i+=1){
        MPI_Status status;
        int data;
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, READY_FOR_NEW_TASK, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(data);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (senderType % 3) {
            case GLOWA:
                insertToQueue(&glowaTeamQueue, status.MPI_SOURCE, data);
                break;
            case OGON:
                insertToQueue(&ogonTeamQueue, status.MPI_SOURCE, data);
                break;
            case TULOW:
                insertToQueue(&tulowTeamQueue, status.MPI_SOURCE, data);
                break;
        }

    }

    showQueue(glowaTeamQueue, GLOWA);
    showQueue(ogonTeamQueue, OGON);
    showQueue(tulowTeamQueue, TULOW);

}

void *workerMainFunc(void *ptr) {
    professionalistsBroadcast(READY_FOR_NEW_TASK);
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);
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

void professionInit(){
    otherTS = malloc(sizeof(int) * size);
    for (int i = 0; i < size; i++) otherTS = 0;
    glowaTeamQueue = (QueueElementType *)malloc(sizeof(QueueElementType));
    ogonTeamQueue = (QueueElementType *)malloc(sizeof(QueueElementType));
    tulowTeamQueue = (QueueElementType *)malloc(sizeof(QueueElementType));
    glowaTeamQueue=NULL;
    ogonTeamQueue=NULL;
    tulowTeamQueue=NULL;
    pthread_mutex_init(&lamportLock, NULL);
}

void professionEnd(){
    pthread_mutex_destroy(&lamportLock);
}

void zleceniodawca(){
    sleep(2);
    singleProfessionBroadcast(TULOW, NEW_TASK);
}

void glowa() {
    professionInit();

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerMainFunc, 0);
    messanger();
    pthread_join(workerThread, NULL);

    professionEnd();
}

void tulow(){
    professionInit();

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerMainFunc, 0);
    messanger();
    pthread_join(workerThread, NULL);

    professionEnd();
}

void ogon(){
    professionInit();

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerMainFunc, 0);
    messanger();
    pthread_join(workerThread, NULL);

    professionEnd();
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);

    enum type profession = getProfession(rank);
    printf("[PROCES %d - MAIN] type %s \n", rank, typeNames[profession]);

    switch(profession){
        case ZLECENIEDOWCA:
            zleceniodawca();
            break;

        case GLOWA:
            glowa();
            break;

        case TULOW:
            tulow();
            break;

        case OGON:
            ogon();
            break;

        default:
            break;
    }

    printf("[PROCES %d - MAIN] koniec Lamport: %d\n", rank, lamportTS);
    MPI_Finalize();
}
