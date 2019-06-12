#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>

#define END 0
#define NEW_TASK 100
#define READY_TO_JOIN_TEAM 200
#define REQUEST_DESK 300
#define REPLY_DESK 400
#define CAN_REQUEST_SKELETON 500
#define REQUEST_SKELETON 600
#define REPLY_SKELETON 700
#define RESURRECTION_START 800
#define RESSURECTION_END 900

int LICZBA_BIUREK = 1, LICZBA_SZKIELETOW = 1;
int liczbaGlow, liczbaTulowi, liczbaOgonow;

pthread_mutex_t lamportLock, lamportArrayLock;

//mutexy tułowia
pthread_mutex_t paperWorkMutex, resurrectTulowMutex, tulowFinishResurrectingMutex, acquireDeskMutex;

//mutexy ogona
pthread_mutex_t skeletonMutex, resurrectOgonMutex, ogonFinishResurrectingMutex, acquireSkeletonMutex;

//mutexy glowy
pthread_mutex_t glowaFinishResurrectingMutex, resurrectGlowaMutex;

int glowaTeamId, ogonTeamId, tulowTeamId;
int myProfession;

enum type {
    OGON, GLOWA, TULOW, ZLECENIEDOWCA
};
const char *typeNames[] = {"OGON", "GLOWA", "TULOW", "ZLECENIEDOWCA"};

typedef enum {
    TRUE = 1, FALSE = 0
} bool;

int rank, size, lamportTS = 0, *otherTS;
bool end = FALSE;
bool working = FALSE;

//zmienne tulowia
bool waitingForDesk = FALSE;
int deskAccessPermissions = 0;

//zmienne ogona
bool waitingForSkeleton = FALSE;
int skeletonAccessPermissions = 0;

typedef struct QueueElement {
    int pID;
    int pLamport;
    struct QueueElement *next;
} QueueElementType;

QueueElementType *glowaTeamQueue, *ogonTeamQueue, *tulowTeamQueue, *deskRequesters, *skeletonRequesters;


int getProcessOnQueueHead(QueueElementType **head) {
    QueueElementType *current = *head;
    if (current != NULL) {
        return current->pID;
    } else {
        printf("[PROCESS %d (%s) - MESSANGER] ERROR. Getting head from empty queue!", rank, typeNames[myProfession]);
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

void removeFirstNode(QueueElementType **head, int count) {
    for (int i = 0; i < count; i++) {
//        printf("[PROCES %d (%s) - MESSANGER] proces %d ma drużynę \n", rank, typeNames[myProfession], (*head)->pID);
        QueueElementType *tmp = (*head);
        (*head) = (*head)->next;
        free(tmp);
    }
}

void removeProcessFromQueue(QueueElementType **head, int processID) {
    if((*head)->pID == processID){
        removeFirstNode(head, 1);
    } else {
        QueueElementType *current = *head;
        while (current->next != NULL) {
            if (current->next->pID == processID) {
                QueueElementType *tmp = current->next;
                current->next = current->next->next;
                free(tmp);
                break;
            }
            current = current->next;
        }
    }
}

void checkForTeams() {
    //ilu specjalistow z kazdej profesji NA PEWNO ma druzyne
    int ileOgonow = getReadyElementsFromQueue(&ogonTeamQueue, OGON);
    int ileGlow = getReadyElementsFromQueue(&glowaTeamQueue, GLOWA);
    int ileTulowi = getReadyElementsFromQueue(&tulowTeamQueue, TULOW);

    //jezeli jakas druzyna jest uformowana
    if (ileOgonow >= 1 && ileGlow >=1 && ileTulowi >= 1) {

        int myPositionInQueue = -1;
        switch(myProfession){
            case OGON:
                myPositionInQueue = getPosInQueue(&ogonTeamQueue, rank);
                break;
            case GLOWA:
                myPositionInQueue = getPosInQueue(&glowaTeamQueue, rank);
                break;
            case TULOW:
                myPositionInQueue = getPosInQueue(&tulowTeamQueue, rank);
                break;
            default:
                break;
        }

        //jestem pierwszy w kolejce
        if (myPositionInQueue == 1) {
            //zapisz druzyne
            ogonTeamId = getProcessOnQueueHead(&ogonTeamQueue);
            glowaTeamId = getProcessOnQueueHead(&glowaTeamQueue);
            tulowTeamId = getProcessOnQueueHead(&tulowTeamQueue);
            printf("[PROCES %d (%s) - MESSANGER] Jestem 1 w kolejce. Moja drużyna: Tulow - %d, Ogon - %d, Glowa - %d\n", rank, typeNames[myProfession], tulowTeamId, ogonTeamId, glowaTeamId);


            //pozwol tulowiu ubiegac sie o biurko
            if(myProfession == TULOW) pthread_mutex_unlock(&acquireDeskMutex);
        } else if (myPositionInQueue < 0){
            if(myProfession == TULOW) printf("[PROCES %d (%s) - MESSANGER] Pracuję. Nie rozpoczynam ubiegania się o biurko\n", rank, typeNames[myProfession]);
        } else {
            if(myProfession == TULOW) printf("[PROCES %d (%s) - MESSANGER] Jestem %d w kolejce. Nie rozpoczynam ubiegania się o biurko\n", rank, typeNames[myProfession], myPositionInQueue);
        }

        removeFirstNode(&ogonTeamQueue, 1);
        removeFirstNode(&glowaTeamQueue, 1);
        removeFirstNode(&tulowTeamQueue, 1);
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
    printf("[PROCES %d (%s) - QUEUE] %s: ", rank, typeNames[myProfession], typeNames[type]);
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

void sendToAllInQueueWithoutMe(QueueElementType **head, int me, int msg) {
    QueueElementType *current = *head;
    while (current != NULL) {
        if (current->pID != me) {
            pthread_mutex_lock(&lamportLock);
            lamportIncreaseBeforeSend();
            MPI_Send(&lamportTS, 1, MPI_INT, current->pID, msg, MPI_COMM_WORLD);
            pthread_mutex_unlock(&lamportLock);
        }
        current = current->next;
    }
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

void singleProfessionBroadcastWithoutOneProcess(enum type sendTo, int task, int sendNotTo) {
    pthread_mutex_lock(&lamportLock);
    lamportIncreaseBeforeSend();
    for (int processId = sendTo + 1; processId < size; processId += 3) {
        if (processId != sendNotTo) MPI_Send(&lamportTS, 1, MPI_INT, processId, task, MPI_COMM_WORLD);
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

void onReadyToJoinTeam(enum type senderType, MPI_Status status, int data) {
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
    //dobierz druzyne do przyjecia zlecenia
    checkForTeams();
}

void onEnd() {
    end = TRUE;
}

void onResurrectionStart(pthread_mutex_t *resurrectMutex){
    printf("[PROCES %d (%s) - MESSANGER] Dostałem pozwolenie na wskrzeszanie\n", rank, typeNames[myProfession]);
    pthread_mutex_unlock(resurrectMutex);
}

void messangerGlowy() {

    while (end == FALSE) {
        MPI_Status status;
        int senderLts;
        MPI_Recv(&senderLts, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(senderLts, status.MPI_SOURCE);
        printf("[PROCES %d (%s) - MESSANGER] dostal wiadomosc %d z TS: %d od %d\n", rank, typeNames[myProfession], status.MPI_TAG, senderLts, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case READY_TO_JOIN_TEAM:
                onReadyToJoinTeam(senderType, status, senderLts);
                break;
            case NEW_TASK:
                onNewTask();
                break;
            case RESURRECTION_START:
                onResurrectionStart(&resurrectGlowaMutex);
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

void onReplyDesk() {
    deskAccessPermissions += 1;

    //uzyskano dostep do biurka
    if (deskAccessPermissions >= liczbaTulowi - LICZBA_BIUREK) {
        printf("[PROCES %d (%s) - MESSANGER] Uzyskałem wymagany dostęp do biurka\n", rank, typeNames[myProfession]);
        //TODO: CO TUTAJ Z NASZĄ KOLEJKĄ BIUREK?? CZYŚCIMY??
        pthread_mutex_unlock(&paperWorkMutex);
    } else {
        printf("[PROCES %d (%s) - MESSANGER] Uzyskałem pozwolenie na dostep do biurka. Mam %d / %d \n", rank, typeNames[myProfession], deskAccessPermissions, liczbaTulowi - LICZBA_BIUREK);
    }
}

void onRequestDesk(int senderRank, int senderLts) {
    if (waitingForDesk == FALSE) {
        //nie czeka na biurko pozwala wejść
        lamportIncreaseBeforeSend();
        MPI_Send(&lamportTS, 1, MPI_INT, senderRank, REPLY_DESK, MPI_COMM_WORLD);
    } else {
        //czekamy na biurko, wrzucamy sendera do kolejki
        insertToQueue(&deskRequesters, senderRank, senderLts);

        int myPosInQueue = getPosInQueue(&deskRequesters, rank);
        int hisPosInQueue = getPosInQueue(&deskRequesters, senderRank);

        if (myPosInQueue < hisPosInQueue && hisPosInQueue >= 0) {
            //jesli jestesmy nizej w kolejce pozwalamy wejsc i usuwamy z kolejki
            //wpw odpowiemy jak zwolnimy dostęp
            removeProcessFromQueue(&deskRequesters, senderRank);
            lamportIncreaseBeforeSend();
            MPI_Send(&lamportTS, 1, MPI_INT, senderRank, REPLY_DESK, MPI_COMM_WORLD);
        }
    }
}

void messangerTulowia() {

    while (end == FALSE) {
        MPI_Status status;
        int senderLts;
        MPI_Recv(&senderLts, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(senderLts, status.MPI_SOURCE);
        printf("[PROCES %d (%s) - MESSANGER] dostal wiadomosc %d z TS: %d od %d\n", rank, typeNames[myProfession], status.MPI_TAG, senderLts, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case NEW_TASK:
                onNewTask();
                break;
            case READY_TO_JOIN_TEAM:
                onReadyToJoinTeam(senderType, status, senderLts);
                break;
            case REPLY_DESK:
                onReplyDesk();
                break;
            case REQUEST_DESK:
                onRequestDesk(status.MPI_SOURCE, senderLts);
                break;
            case RESURRECTION_START:
                onResurrectionStart(&resurrectTulowMutex);
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

void onCanRequestSkeleton() {
    printf("[PROCES %d (%s) - MESSANGER] Dostałem pozwolenie na ubieganie się o szkielet\n", rank, typeNames[myProfession]);
    pthread_mutex_unlock(&acquireSkeletonMutex);
}

void onRequestSkeleton(int senderRank, int senderLts) {
    if (waitingForSkeleton == FALSE) {
        //nie czeka na szkielet pozwala wejść
        lamportIncreaseBeforeSend();
        MPI_Send(&lamportTS, 1, MPI_INT, senderRank, REPLY_SKELETON, MPI_COMM_WORLD);
    } else {
        //czekamy na szkielet, wrzucamy sendera do kolejki
        insertToQueue(&skeletonRequesters, senderRank, senderLts);

        int myPosInQueue = getPosInQueue(&skeletonRequesters, rank);
        int hisPosInQueue = getPosInQueue(&skeletonRequesters, senderRank);

        if (myPosInQueue < hisPosInQueue && hisPosInQueue >= 0) {
            //jesli jestesmy nizej w kolejce pozwalamy wejsc i usuwamy z kolejki
            //wpw odpowiemy jak zwolnimy dostęp
            removeProcessFromQueue(&skeletonRequesters, senderRank);
            lamportIncreaseBeforeSend();
            MPI_Send(&lamportTS, 1, MPI_INT, senderRank, REPLY_SKELETON, MPI_COMM_WORLD);
        }
    }
}

void onReplySkeleton() {
    skeletonAccessPermissions += 1;

    //uzyskano dostep do szkieletus
    if (skeletonAccessPermissions >= liczbaOgonow - LICZBA_SZKIELETOW) {
        printf("[PROCES %d (%s) - MESSANGER] Uzyskałem wymagany dostęp do szkieletu\n", rank, typeNames[myProfession]);
        //TODO: CO TUTAJ Z NASZĄ KOLEJKĄ SZKIELETOW?? CZYŚCIMY??
        pthread_mutex_unlock(&skeletonMutex);
    } else {
        printf("[PROCES %d (%s) - MESSANGER] Uzyskałem pozwolenie na dostep do szkieletu. Mam %d / %d \n", rank, typeNames[myProfession], skeletonAccessPermissions, liczbaOgonow - LICZBA_SZKIELETOW);
    }
}

void sendResurrectionPermit(int processID){
    printf("[PROCES %d (%s) - WORKERTHREAD] Informuję proces %d że może zacząc wskrzeszac\n", rank, typeNames[myProfession], processID);
    lamportIncreaseBeforeSend();
    MPI_Send(&lamportTS, 1, MPI_INT, processID, RESURRECTION_START, MPI_COMM_WORLD);
};

void messangerOgona() {

    while (end == FALSE) {
        MPI_Status status;
        int senderLts;
        MPI_Recv(&senderLts, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportIncreaseAfterRecv(senderLts, status.MPI_SOURCE);
        printf("[PROCES %d (%s) - MESSANGER] dostal wiadomosc %d z TS: %d od %d\n", rank, typeNames[myProfession], status.MPI_TAG, senderLts, status.MPI_SOURCE);
        enum type senderType = getProfession(status.MPI_SOURCE);
        switch (status.MPI_TAG) {
            case READY_TO_JOIN_TEAM:
                onReadyToJoinTeam(senderType, status, senderLts);
                break;
            case NEW_TASK:
                onNewTask();
                break;
            case CAN_REQUEST_SKELETON:
                onCanRequestSkeleton();
                break;
            case REQUEST_SKELETON:
                onRequestSkeleton(status.MPI_SOURCE, senderLts);
                break;
            case REPLY_SKELETON:
                onReplySkeleton();
                break;
            case RESURRECTION_START:
                onResurrectionStart(&resurrectOgonMutex);
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

void doResurrection(){
    printf("[PROCES %d (%s) - WORKTHREAD] rozpoczynam wskrzeszanie\n", rank, typeNames[myProfession]);
    sleep(3);
    printf("[PROCES %d (%s) - WORKTHREAD] zakończyłem wskrzeszanie\n", rank, typeNames[myProfession]);
}

void *workerGlowy(void *ptr) {
    printf("[PROCES %d (%s) - WORKERTHREAD] start \n", rank, typeNames[myProfession]);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij wskrzeszanie gdy dostaniesz informacje od ogona, poinformuj tulow gdy skonczysz
        pthread_mutex_lock(&resurrectGlowaMutex);
        doResurrection();
        sendResurrectionPermit(tulowTeamId);
        pthread_mutex_unlock(&resurrectGlowaMutex);

        //poczkaj na informacje od ogona o zakonczeniu wskrzeszania
        pthread_mutex_lock(&glowaFinishResurrectingMutex);

        //TODO: podbić licznik wskrzeszeń

        pthread_mutex_unlock(&glowaFinishResurrectingMutex);

        //TODO: zacząć ubiegać się o drużynę
    }
}

void acquireDesk() {
    printf("[PROCES %d (%s) - WORKERTHREAD] Rozpoczynam ubieganie się o biurko\n", rank, typeNames[myProfession]);
    singleProfessionBroadcastWithoutOneProcess(TULOW, REQUEST_DESK, rank);
    waitingForDesk = TRUE;
    insertToQueue(&deskRequesters, rank, lamportTS);
}

void doPaperWork() {
    printf("[PROCES %d (%s) - WORKERTHREAD] Wykonuje papierkową robotę\n", rank, typeNames[myProfession]);
    sleep(3);
    waitingForDesk = FALSE;
//    sendToAllInQueueWithoutMe(&deskRequesters, rank, REPLY_DESK);
    lamportIncreaseBeforeSend();
    MPI_Send(&lamportTS, 1, MPI_INT, ogonTeamId, CAN_REQUEST_SKELETON, MPI_COMM_WORLD);
}

void *workerTulowia(void *ptr) {
    printf("[PROCES %d (%s) - WORKERTHREAD] start \n", rank, typeNames[myProfession]);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij ubiegać się o biurko kiedy masz drużynę
        pthread_mutex_lock(&acquireDeskMutex);
        acquireDesk();
        pthread_mutex_unlock(&acquireDeskMutex);

        //zacznij robotę papierkową kiedy będziesz miał biurko
        pthread_mutex_lock(&paperWorkMutex);
        doPaperWork();
        pthread_mutex_unlock(&paperWorkMutex);

        //zacznij wskrzeszanie tulowia kiedy głowa da znać, poinformuj ogon gdy skonczysz
        pthread_mutex_lock(&resurrectTulowMutex);
        doResurrection();
        sendResurrectionPermit(ogonTeamId);
        pthread_mutex_unlock(&resurrectTulowMutex);

        //skoncz wskrzeszanie gdy ogon da znac
        pthread_mutex_lock(&tulowFinishResurrectingMutex);

        //TODO: podbić licznik wskrzeszeń

        pthread_mutex_unlock(&tulowFinishResurrectingMutex);
    }
}

void acquireSkeleton() {
    printf("[PROCES %d (%s) - WORKERTHREAD] Rozpoczynam ubieganie się o szkielet\n", rank, typeNames[myProfession]);
    singleProfessionBroadcastWithoutOneProcess(OGON, REQUEST_SKELETON, rank);
    waitingForSkeleton = TRUE;
    insertToQueue(&skeletonRequesters, rank, lamportTS);
}

void endOfResurrection(){
    printf("[PROCES %d (%s) - WORKERTHREAD] SMOK ZOSTAŁ WSKRZESZONY\n", rank, typeNames[myProfession]);
}

void *workerOgona(void *ptr) {
    printf("[PROCES %d (%s) - WORKERTHREAD] start \n", rank, typeNames[myProfession]);

    while (end == FALSE) {
        //ogłoś, że jesteś gotowy dołączyć do drużyny
        allProfessionsBroadcast(READY_TO_JOIN_TEAM);

        //zacznij ubiegać się o szkielet kiedy tułów skończy papierkowa robote
        pthread_mutex_lock(&acquireSkeletonMutex);
        acquireSkeleton();
        pthread_mutex_unlock(&acquireSkeletonMutex);

        //gdy otrzymasz dostęp do szkieletu wyślij głowie informację że może rozpocząc wskrzeszanie
        pthread_mutex_lock(&skeletonMutex);
        sendResurrectionPermit(glowaTeamId);
        pthread_mutex_unlock(&skeletonMutex);

        //zacznij wskrzeszanie gdy otrzymasz informacje od tulowia
        pthread_mutex_lock(&resurrectOgonMutex);
        doResurrection();
        endOfResurrection();
        pthread_mutex_unlock(&resurrectOgonMutex);

        pthread_mutex_lock(&ogonFinishResurrectingMutex);

        pthread_mutex_unlock(&ogonFinishResurrectingMutex);
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
//NARK
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
    deskRequesters = (QueueElementType *) malloc(sizeof(QueueElementType));
    skeletonRequesters = (QueueElementType *) malloc(sizeof(QueueElementType));
    glowaTeamQueue = NULL;
    ogonTeamQueue = NULL;
    tulowTeamQueue = NULL;
    deskRequesters = NULL;
    skeletonRequesters = NULL;
    pthread_mutex_init(&lamportLock, NULL);
}

void professionEnd() {
    pthread_mutex_destroy(&lamportLock);
}

void zleceniodawca() {
    //w losowych odstępuj generuj zlecenia i wysyłaj je do wszystkich tułowi
    sleep(5);
    printf("[PROCES %d (%s) - MAIN] wysłałem nowe zlecenie\n", rank, typeNames[myProfession]);
    allProfessionsBroadcast(NEW_TASK);
    sleep(5);
    printf("[PROCES %d (%s) - MAIN] wysłałem nowe zlecenie\n", rank, typeNames[myProfession]);
    allProfessionsBroadcast(NEW_TASK);
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
    pthread_mutex_init(&ogonFinishResurrectingMutex, NULL);

    // skeletonMutex, resurrectOgonMutex, ogonFinishResurrectingMutex, acquireSkeletonMutex;

    pthread_mutex_lock(&acquireSkeletonMutex);
    pthread_mutex_lock(&skeletonMutex);
    pthread_mutex_lock(&resurrectOgonMutex);
    pthread_mutex_lock(&ogonFinishResurrectingMutex);
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

    removeFirstNode(&teamQueue, count);
    removeFirstNode(&teamQueue2, count);

    printf("[TEST] ile: %d %d \n", ile, ile2);

    printf("[PO USUNIECIU]\n");

    showQueue(teamQueue, OGON);
    showQueue(teamQueue2, GLOWA);
}

void setProfessionCount() {
    if (size % 3 == 1) {
        liczbaGlow = size / 3;
        liczbaOgonow = size / 3 ;
        liczbaTulowi = size / 3;
    } else if (size % 3 == 2) {
        liczbaGlow = size / 3;
        liczbaOgonow = size / 3 + 1;
        liczbaTulowi = size / 3;
    } else {
        liczbaGlow = size / 3 + 1;
        liczbaOgonow = size / 3 + 1;
        liczbaTulowi = size / 3;
    }
    printf("[PROCES %d (%s) - MAIN] liczba Ogonow - %d, liczba Glow - %d, liczba Tulowi - %d\n", rank, typeNames[myProfession], liczbaOgonow, liczbaGlow, liczbaTulowi);
}

int main(int argc, char **argv) {
    int providedThreadSystem = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &providedThreadSystem);

    initThreadSystem(providedThreadSystem, &rank, &size);
    setProfessionCount();
//    test();

    //nadaj procesowi profesję
    enum type profession = getProfession(rank);
    myProfession = profession;
//    printf("[PROCES %d - MAIN] type %s \n", rank, typeNames[myProfession]);

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

    printf("[PROCES %d (%s) - MAIN] koniec Lamport: %d\n", rank, typeNames[profession],lamportTS);
    MPI_Finalize();
}
