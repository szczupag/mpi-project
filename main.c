#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>

#define NEW_TASK 100
#define READY_TO_JOIN_TEAM 200
#define END 0
#define REQUEST_DESK 300
#define REPLY_DESK 400

pthread_mutex_t lamportLock, lamportArrayLock;

//mutexy tułowia
pthread_mutex_t paperWorkMutex, resurrectTulowMutex, tulowFinishResurrectingMutex, acquireDeskMutex;

//mutexy ogona
pthread_mutex_t skeletonMutex, resurrectOgonMutex, acquireSkeletonMutex;

//mutexy glowy
pthread_mutex_t glowaFinishResurrectingMutex, resurrectGlowaMutex;

int glowaTeamId, ogonTeamId;

enum type {
    OGON, GLOWA, TULOW, ZLECENIEDOWCA
};
const char *typeNames[] = {"OGON", "GLOWA", "TULOW", "ZLECENIEDOWCA"};

typedef enum {
    TRUE = 1, FALSE = 0
} bool;

int rank, size, lamportTS = 0, *otherTS;
bool end = FALSE;

typedef struct QueueElement {
    int pID;
    int pLamport;
    struct QueueElement *next;
} QueueElementType;

QueueElementType *glowaTeamQueue, *ogonTeamQueue, *tulowTeamQueue;


int getProcessOnQueueHead(QueueElementType **head) {
    QueueElementType *current = *head;
    if (current != NULL) {
        return current->pID;
    } else {
        printf("[PROCESS %d] ERROR. Getting head from empty queue!", rank);
        return -1;
    }
}

bool isInQueue(QueueElementType **head, int processID) {
    QueueElementType *current = *head;
    bool isIn = FALSE;
    while (current != NULL) {
        if (current->pID == processID) {
            isIn = TRUE;
            break;
        }
        current = current->next;
    }
    return isIn;
}

int getPosInQueue(QueueElementType **head, int processID) {
    QueueElementType *current = *head;
    int position = -1, counter = 0;
    while (current != NULL) {
        counter += 1;
        if (current->pID == processID) {
            position = counter;
            break;
        }
        current = current->next;
    }
    return position;
}

int getReadyElementsFromQueue(QueueElementType **head, enum type profession) {
    int min = 100, canEnter = 0;
    for (int i = profession + 1; i < size; i += 3) {
        if (otherTS[i] < min && isInQueue(head, i) == FALSE) {
            min = otherTS[i];
        }
    }

    QueueElementType *current = *head;
    while (current != NULL && current->pLamport <= min) {
        canEnter += 1;
        current = current->next;
    }
    return canEnter;
}

void removeFirstNodes(QueueElementType **head, int count) {
    for (int i = 0; i < count; i++) {
        printf("[PROCES %d] proces %d ma drużynę \n", rank, (*head)->pID);
        QueueElementType *tmp = (*head);
        (*head) = (*head)->next;
        free(tmp);
    }
}

void checkForTeams() {
    //ilu specjalistow z kazdej profesji NA PEWNO ma druzyne
    int ileOgonow = getReadyElementsFromQueue(&ogonTeamQueue, OGON);
    int ileGlow = getReadyElementsFromQueue(&glowaTeamQueue, GLOWA);
    int ileTulowi = getReadyElementsFromQueue(&tulowTeamQueue, TULOW);

    //jezeli jakas druzyna jest uformowana
    if (ileOgonow >= 1 && ileGlow >=1 && ileTulowi >= 1) {

        int myPositionInQueue = getPosInQueue(&tulowTeamQueue, rank);

        //jestem pierwszy w kolejce
        if (myPositionInQueue == 1) {
            //zapisz druzyne
            ogonTeamId = getProcessOnQueueHead(&ogonTeamQueue);
            glowaTeamId = getProcessOnQueueHead(&glowaTeamQueue);
            printf("[PROCES %d] Jestem 1 w kolejce. Moja drużyna: Tulow - %d, Ogon - %d, Glowa - %d\n", rank, rank, ogonTeamId, glowaTeamId);

            //pozwol ubiegac sie o biurko
            pthread_mutex_unlock(&acquireDeskMutex);
        } else {
            printf("[PROCES %d] Jestem %d w kolejce. Nie rozpoczynam ubiegania się o biurko\n", rank, myPositionInQueue);
        }

        removeFirstNodes(&ogonTeamQueue, 1);
        removeFirstNodes(&glowaTeamQueue, 1);
        removeFirstNodes(&tulowTeamQueue, 1);
    }
}

void insertAfter(QueueElementType *current, int processID, int processLamport) {
    QueueElementType *tmp = current->next;
    current->next = (QueueElementType *) malloc(sizeof(QueueElementType));
    current->next->pID = processID;
    current->next->pLamport = processLamport;
    current->next->next = tmp;
}

void insertToQueue(QueueElementType **head, int processID, int processLamport) {
    if (*head == NULL) {
        *head = (QueueElementType *) malloc(sizeof(QueueElementType));
        (*head)->pID = processID;
        (*head)->pLamport = processLamport;
        (*head)->next = NULL;
    } else if ((*head)->pLamport > processLamport ||
               ((*head)->pLamport == processLamport && (*head)->pID > processID)) {
        QueueElementType *new = (QueueElementType *) malloc(sizeof(QueueElementType));
        new->pLamport = processLamport;
        new->pID = processID;
        new->next = *head;
        (*head) = new;
    } else {
        QueueElementType *current = *head;
        while (current->next != NULL && (current->next->pLamport < processLamport ||
                                         (current->next->pLamport == processLamport &&
                                          current->next->pID < processID))) {
            current = current->next;
        }
        insertAfter(current, processID, processLamport);
    }
}

void showQueue(QueueElementType *head, int type) {
    printf("[PROCES %d - QUEUE] %s: ", rank, typeNames[type]);
    if (head == NULL) printf("Queue is empty");
    else {
        QueueElementType *current = head;
        do {
            printf("{ procesID: %d, lamportTS: %d }, ", current->pID, current->pLamport);
            current = current->next;
        } while (current != NULL);
    }
    printf("\n");
}

void lamportIncreaseAfterRecv(int senderLTS, int senderRank) {
    pthread_mutex_lock(&lamportLock);
    otherTS[senderRank] = senderLTS;
    if (senderLTS > lamportTS) {
        lamportTS = senderLTS + 1;
    } else {
        lamportTS += 1;
    }
    pthread_mutex_unlock(&lamportLock);
}

void lamportIncreaseBeforeSend() {
    lamportTS += 1;
}

void allProfessionsBroadcast(int task) {
    pthread_mutex_lock(&lamportLock);
    lamportIncreaseBeforeSend();
    for (int processId = 1; processId < size; processId += 1) {
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

void onReadyForNewTask(enum type senderType, MPI_Status status, int data) {
    //dodaj procesy chętne do uformowania drużyny do kolejek drużyn
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

void onNewTask() {
    //jeżeli masz drużynę i możesz wejść do sekcji krtycznej, zacznij ubiegać się o szkielet
    checkForTeams();
}

void onEnd() {
    end = TRUE;
}

void messangerGlowy() {

    while (end == FALSE) {
        MPI_Status status;
        int data;
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(data, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, data, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case NEW_TASK:
                onNewTask();
                break;
            case READY_TO_JOIN_TEAM:
                onReadyForNewTask(senderType, status, data);
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

void messangerTulowia() {

    while (end == FALSE) {
        MPI_Status status;
        int senderLts;
        MPI_Recv(&senderLts, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(senderLts, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, senderLts, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case NEW_TASK:
                onNewTask();
                break;
            case READY_TO_JOIN_TEAM:
                onReadyForNewTask(senderType, status, senderLts);
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

void messangerOgona() {

    while (end == FALSE) {
        MPI_Status status;
        int senderLts;
        MPI_Recv(&senderLts, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(senderLts, status.MPI_SOURCE);
        printf("[PROCES %d - MESSANGER] dostal %d od %d\n", rank, senderLts, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case NEW_TASK:
                onNewTask();
                break;
            case READY_TO_JOIN_TEAM:
                onReadyForNewTask(senderType, status, senderLts);
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
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij wskrzeszanie gdy dostaniesz informacje od ogona
        pthread_mutex_lock(&resurrectGlowaMutex);

        //wskrzeszanie i czekanko

        pthread_mutex_unlock(&resurrectGlowaMutex);

        //TODO: poinformuj tułów, żeby zaczął wskrzeszać

        //poczkaj na informacje od ogona o zakonczeniu wskrzeszania
        pthread_mutex_lock(&glowaFinishResurrectingMutex);

        //TODO: podbić licznik wskrzeszeń

        pthread_mutex_unlock(&glowaFinishResurrectingMutex);

        //TODO: zacząć ubiegać się o drużynę
    }
}

void *workerTulowia(void *ptr) {
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij ubiegać się o biurko kiedy masz drużynę
        pthread_mutex_lock(&acquireDeskMutex);
        printf("[PROCES %d - WORKERTHREAD] Rozpoczynam ubieganie się o biurko\n", rank);
        //TODO: REQUEST dostępu do biurka do pozostałych tułowi
        pthread_mutex_unlock(&acquireDeskMutex);

        //zacznij robotę papierkową kiedy będziesz miał biurko
        pthread_mutex_lock(&paperWorkMutex);

        //robota papierkowa i spanko

        //koniec roboty papierkowej
        pthread_mutex_unlock(&paperWorkMutex);

        //TODO: REPLY do pozostalych tułowi ubierających się o biurko
        //TODO: poinformować ogon, że ma zacząć ubiegać się o szkielet.

        //zacznij wskrzeszanie tulowia kiedy głowa da znać
        pthread_mutex_lock(&resurrectTulowMutex);

        //wskrzeszanie i spanko

        //koniec wskrzeszania
        pthread_mutex_unlock(&resurrectTulowMutex);
        //TODO: poinformować ogon, że ma rozpocząć wskrzeszanie swojej części

        //skoncz wskrzeszanie gdy ogon da znac
        pthread_mutex_lock(&tulowFinishResurrectingMutex);

        //TODO: podbić licznik wskrzeszeń

        pthread_mutex_unlock(&tulowFinishResurrectingMutex);

        //TODO: ogłosic ze szuka sie druzyny
    }
}

void *workerOgona(void *ptr) {
    printf("[PROCES %d - WORKERTHREAD] start \n", rank);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij ubiegać się o szkielet kiedy tułów skończy papierkowa robote
        pthread_mutex_lock(&acquireSkeletonMutex);

        //TODO: REQUEST dostępu do szkieletu do pozostałych ogonów

        //koniec ubiegania się o szkielet
        pthread_mutex_unlock(&acquireSkeletonMutex);

        //gdy otrzymasz dostęp do szkieleta
        pthread_mutex_lock(&skeletonMutex);

        //TODO: wysłać do głowy informację o rozpoczęciu wskrzeszania

        pthread_mutex_unlock(&skeletonMutex);

        //zacznij wskrzeszanie gdy otrzymasz informacje od tulowia
        pthread_mutex_lock(&resurrectOgonMutex);

        //wskrzeszanie i spanko

        pthread_mutex_unlock(&resurrectOgonMutex);
        //TODO: REPLY to pozostałych ogonów czekających na szkielet
        //TODO: wysłać do głowy i tułowia informację o zakończeniu wskrzeszania, podbić licznik wskrzeszeń
        //TODO: ogłosic ze szuka sie druzyny
    }

}

void initThreadSystem(int threadSystem, int *processRank, int *size) {
    if (threadSystem != MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
        MPI_Finalize();
        exit(-1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, processRank);
    MPI_Comm_size(MPI_COMM_WORLD, size);
}

//utwórz kolejki do dobierania drużyn i tablice do trzymania informacji o timestampach pozostałych procesów
void professionInit() {
    otherTS = malloc(sizeof(int) * size);
    for (int i = 0; i < size; i++) otherTS[i] = 0;
    glowaTeamQueue = (QueueElementType *) malloc(sizeof(QueueElementType));
    ogonTeamQueue = (QueueElementType *) malloc(sizeof(QueueElementType));
    tulowTeamQueue = (QueueElementType *) malloc(sizeof(QueueElementType));
    glowaTeamQueue = NULL;
    ogonTeamQueue = NULL;
    tulowTeamQueue = NULL;
    pthread_mutex_init(&lamportLock, NULL);
}

void professionEnd() {
    pthread_mutex_destroy(&lamportLock);
}

void zleceniodawca() {
    //w losowych odstępuj generuj zlecenia i wysyłaj je do wszystkich tułowi
    sleep(5);
    singleProfessionBroadcast(TULOW, NEW_TASK);
    sleep(5);
    singleProfessionBroadcast(TULOW, NEW_TASK);
}

void initGlowaMutexes() {
    pthread_mutex_init(&glowaFinishResurrectingMutex, NULL);
    pthread_mutex_init(&resurrectGlowaMutex, NULL);

    pthread_mutex_lock(&glowaFinishResurrectingMutex);
    pthread_mutex_lock(&resurrectGlowaMutex);
}

void glowa() {
    //inicjalizacja kolejek
    professionInit();

    //inicjalizcja mutexow
    initGlowaMutexes();

    //utworz watek do komunikacji i odpal glowna funkcje
    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerGlowy, 0);
    messangerGlowy();
    pthread_join(workerThread, NULL);

    //zakoncz dzialanie
    professionEnd();
}

void initTulowMutexes() {
    pthread_mutex_init(&paperWorkMutex, NULL);
    pthread_mutex_init(&resurrectTulowMutex, NULL);
    pthread_mutex_init(&tulowFinishResurrectingMutex, NULL);
    pthread_mutex_init(&acquireDeskMutex, NULL);

    pthread_mutex_lock(&paperWorkMutex);
    pthread_mutex_lock(&resurrectTulowMutex);
    pthread_mutex_lock(&tulowFinishResurrectingMutex);
    pthread_mutex_lock(&acquireDeskMutex);
}

void tulow() {
    //incjalizacja kolejek
    professionInit();

    //incjalizacja mutexow
    initTulowMutexes();

    //utworz watek do komunikacji i odpal glowna funkcje
    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerTulowia, 0);
    messangerTulowia();
    pthread_join(workerThread, NULL);

    //zakoncz dzialanie
    professionEnd();
}

void initOgonMutexes() {
    pthread_mutex_init(&skeletonMutex, NULL);
    pthread_mutex_init(&resurrectOgonMutex, NULL);
    pthread_mutex_init(&acquireSkeletonMutex, NULL);

    // skeletonMutex, resurrectOgonMutex, ogonFinishResurrectingMutex, acquireSkeletonMutex;

    pthread_mutex_lock(&acquireSkeletonMutex);
    pthread_mutex_lock(&skeletonMutex);
    pthread_mutex_lock(&resurrectOgonMutex);
}

void ogon() {
    //inicjalizacja kolejek
    professionInit();

    //inicjalizacja mutexów
    initOgonMutexes();

    //utworz watek do komunikacji i odpal glowna funkcje
    pthread_t workerThread;
    pthread_create(&workerThread, NULL, workerOgona, 0);
    messangerOgona();
    pthread_join(workerThread, NULL);

    //zakoncz dzialanie
    professionEnd();
}

void test() {
    QueueElementType *teamQueue = (QueueElementType *) malloc(sizeof(QueueElementType));
    QueueElementType *teamQueue2 = (QueueElementType *) malloc(sizeof(QueueElementType));
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
    if (ile2 < ile) count = ile2;

    removeFirstNodes(&teamQueue, count);
    removeFirstNodes(&teamQueue2, count);

    printf("[TEST] ile: %d %d \n", ile, ile2);

    printf("[PO USUNIECIU]\n");

    showQueue(teamQueue, OGON);
    showQueue(teamQueue2, GLOWA);
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);

//    test();

    //nadaj procesowi profesję
    enum type profession = getProfession(rank);
    printf("[PROCES %d - MAIN] type %s \n", rank, typeNames[profession]);

    //funkcja dla każdej profesji
    switch (profession) {
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
