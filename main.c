#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#define NEW_TASK 100
#define READY_FOR_NEW_TASK 200
#define END 0

pthread_mutex_t lamportLock, lamportArrayLock;

enum type {OGON, GLOWA, TULOW, ZLECENIEDOWCA};
const char* typeNames[] = {"OGON", "GLOWA", "TULOW", "ZLECENIEDOWCA"};

typedef enum {TRUE = 1, FALSE = 0} bool;

int rank, size, lamportTS = 0, *otherTS;
bool end = FALSE;

typedef struct QueueElement {
    int pID;
    int pLamport;
    struct QueueElement * next;
} QueueElementType;

QueueElementType *glowaTeamQueue, *ogonTeamQueue, *tulowTeamQueue;

bool isInQueue(QueueElementType **head, int processID){
    QueueElementType *current=*head;
    bool isIn = FALSE;
    while (current != NULL){
        if(current->pID == processID) {
            isIn = TRUE;
            break;
        }
        current=current->next;
    }
    return isIn;
}

int getReadyElementsFromQueue(QueueElementType **head, enum type profession){
    int min = 100, canEnter = 0;
    for(int i = profession+1; i < size; i+=3 ) {
        if (otherTS[i] < min && isInQueue(head, i) == FALSE) {
            min = otherTS[i];
        }
    }

    QueueElementType *current=*head;
    while (current != NULL && current->pLamport <= min){
        canEnter += 1;
        current = current->next;
    }
    return canEnter;
}

void removeFirstNodes(QueueElementType **head, int count){
    for(int i = 0; i<count; i++){
        QueueElementType *tmp = (*head);
        (*head)=(*head)->next;
        free(tmp);
    }
}

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
    } else if( (*head)->pLamport> processLamport || ((*head)->pLamport== processLamport && (*head)->pID>processID) ){
            QueueElementType *new = (QueueElementType *)malloc(sizeof(QueueElementType));
            new->pLamport= processLamport;
            new->pID = processID;
            new->next = *head;
            (*head) = new;
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

void lamportIncreaseAfterRecv(int senderLTS, int senderRank){
    pthread_mutex_lock(&lamportLock);
    otherTS[senderRank] = senderLTS;
    if(senderLTS>lamportTS){
        lamportTS = senderLTS + 1;
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
    pthread_mutex_lock(&lamportLock);
    lamportIncreaseBeforeSend();
    for (int processId = sendTo + 1; processId < size; processId += 3) {
        MPI_Send(&lamportTS, 1, MPI_INT, processId, task, MPI_COMM_WORLD);
    }
    pthread_mutex_unlock(&lamportLock);
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

void onReadyForNewTask(enum type senderType, MPI_Status status, int data){
    switch (senderType) {
        case GLOWA:
            insertToQueue(&glowaTeamQueue, status.MPI_SOURCE, data);
            break;
        case OGON:
            insertToQueue(&ogonTeamQueue, status.MPI_SOURCE, data);
            break;
        case TULOW:
            insertToQueue(&tulowTeamQueue, status.MPI_SOURCE, data);
            break;
        default:
            break;
    }
}

void onEnd(){
    end = TRUE;
}

void messangerGlowy(){

    while(end == FALSE){
        printf("[PROCES %d - MESSANGER] loop \n", rank);
        MPI_Status status;
        int data;
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(data, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch(status.MPI_TAG){
            case READY_FOR_NEW_TASK:
                onReadyForNewTask(senderType,status,data);
                break;
            case END:
                onEnd();
                break;
            default:
                break;
        }
    }

    showQueue(glowaTeamQueue, GLOWA);
    showQueue(ogonTeamQueue, OGON);
    showQueue(tulowTeamQueue, TULOW);
}

void messangerTulowia(){

    while(end == FALSE){
        printf("[PROCES %d - MESSANGER] loop \n", rank);
        MPI_Status status;
        int data;
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(data, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch(status.MPI_TAG){
            case READY_FOR_NEW_TASK:
                onReadyForNewTask(senderType,status,data);
                break;
            case END:
                onEnd();
                break;
            default:
                break;
        }
    }

    showQueue(glowaTeamQueue, GLOWA);
    showQueue(ogonTeamQueue, OGON);
    showQueue(tulowTeamQueue, TULOW);
}

void messangerOgona(){

    while(end == FALSE){
        printf("[PROCES %d - MESSANGER] loop \n", rank);
        MPI_Status status;
        int data;
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(data, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch(status.MPI_TAG){
            case READY_FOR_NEW_TASK:
                onReadyForNewTask(senderType,status,data);
                break;
            case END:
                onEnd();
                break;
            default:
                break;
        }
    }

    showQueue(glowaTeamQueue, GLOWA);
    showQueue(ogonTeamQueue, OGON);
    showQueue(tulowTeamQueue, TULOW);
}

void *workerGlowy(void *ptr) {
    professionalistsBroadcast(READY_FOR_NEW_TASK);
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);
}

void *workerTulowia(void *ptr) {
    professionalistsBroadcast(READY_FOR_NEW_TASK);
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);
}

void *workerOgona(void *ptr) {
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
    for (int i = 0; i < size; i++) otherTS[i] = 0;
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
    pthread_create(&workerThread, NULL, workerGlowy, 0);
    messangerGlowy();
    pthread_join(workerThread, NULL);

    professionEnd();
}

void tulow(){
    professionInit();

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerTulowia, 0);
    messangerTulowia();
    pthread_join(workerThread, NULL);

    professionEnd();
}

void ogon(){
    professionInit();

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerOgona, 0);
    messangerOgona();
    pthread_join(workerThread, NULL);

    professionEnd();
}

void test(){
    QueueElementType *teamQueue = (QueueElementType *)malloc(sizeof(QueueElementType));
    QueueElementType *teamQueue2 = (QueueElementType *)malloc(sizeof(QueueElementType));
    teamQueue = NULL;
    teamQueue2 = NULL;

    insertToQueue(&teamQueue, 1, 1);
    insertToQueue(&teamQueue, 4, 2);

    insertToQueue(&teamQueue2, 2, 1);
    insertToQueue(&teamQueue2, 5, 2);
    insertToQueue(&teamQueue2, 8, 3);

    otherTS = malloc(sizeof(int) * size);
    for (int i = 0; i < size; i++) otherTS[i] = 0;
    otherTS[1] = 4;
    otherTS[2] = 1;
    otherTS[4] = 7;
    otherTS[5] = 1;
    otherTS[7] = 3;
    otherTS[8] = 1;
    otherTS[10] = 5;
    otherTS[11] = 4;
    otherTS[13] = 6;
    otherTS[14] = 6;

    int ile = getReadyElementsFromQueue(&teamQueue, OGON);
    int ile2 = getReadyElementsFromQueue(&teamQueue2, GLOWA);

    int count = ile;
    if(ile2<ile) count = ile2;

    removeFirstNodes(&teamQueue,count);
    removeFirstNodes(&teamQueue2,count);

    printf("[TEST] ile: %d %d \n",ile, ile2);

    printf("[PO USUNIECIU]\n");

    showQueue(teamQueue, OGON);
    showQueue(teamQueue2, GLOWA);
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);

    printf("[SIZE] %d \n", size);
    test();
//    enum type profession = getProfession(rank);
//    printf("[PROCES %d - MAIN] type %s \n", rank, typeNames[profession]);
//
//    switch(profession){
//        case ZLECENIEDOWCA:
//            zleceniodawca();
//            break;
//
//        case GLOWA:
//            glowa();
//            break;
//
//        case TULOW:
//            tulow();
//            break;
//
//        case OGON:
//            ogon();
//            break;
//
//        default:
//            break;
//    }

    printf("[PROCES %d - MAIN] koniec Lamport: %d\n", rank, lamportTS);
    MPI_Finalize();
}
