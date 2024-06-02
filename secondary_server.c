#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include<sys/shm.h>
#include <pthread.h>
#include <semaphore.h>


#define MAX_MESSAGES 100
#define MAX_MSG_SIZE 100
#define LOAD_BALANCER_MTYPE 104
#define EVEN_SECONDARY_SERVER_MTYPE 102
#define ODD_SECONDARY_SERVER_MTYPE 103
#define FILENAME "./client.c"
#define SHM_BUFFER_SIZE 1024
#define SHM_PERMISSIONS 0666
#define MAX_NODES 30
#define MAX_THREADS 100
#define QUEUE_SIZE 40
#define NUM_FILES 20
#define SEMAPHORE_NAME "/graph-sem"

char names[3][20][2] = {{"aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at"},
                        {"ba", "bb", "bc", "bd", "be", "bf", "bg", "bh", "bi", "bj", "bk", "bl", "bm", "bn", "bo", "bp", "bq", "br", "bs", "bt"},
                        {"ca", "cb", "cc", "cd", "ce", "cf", "cg", "ch", "ci", "cj", "ck", "cl", "cm", "cn", "co", "cp", "cq", "cr", "cs", "ct"}};



// Structure for the message
typedef struct Message {
    long mtype; // Message type (must be greater than 0)
    char mtext[MAX_MSG_SIZE]; // Message data
} Message;


// Structure for semaphore
typedef struct Semaphore {
    sem_t* mutex[NUM_FILES];  // Semaphores for each file
    sem_t* rw_mutex[NUM_FILES];  // Semaphores for each file
    sem_t* in_mutex[NUM_FILES];  // Semaphores for each file
    int counter[NUM_FILES]; 
} Semaphore;
Semaphore semaphore;


// A structure to represent a queue
struct Queue {
    int front, rear, size;
    unsigned capacity;
    int* array;
};


// Structure for passing data to the DFS thread
struct DFSThreadData {
    int adjMatrix[MAX_NODES][MAX_NODES];
    int nodes;
    int startVertex;
    int** visited;
    int** deepestNodes;
    int* countDeep;
};


// Structure for passing data to the BFS thread
struct BFSThreadData {
    int adjMatrix[MAX_NODES][MAX_NODES];
    int nodes;
    int startVertex;
    int** visited;
    struct Queue* queue;
    int listOfVertices[MAX_NODES];
    int* listIndex;
    pthread_mutex_t lock;
};


// Global variables
int qd_lb;        // Message queue descriptor 


// Function to decode message from message queue
void getTokens(int* seq_no, int* op_no, char** g_name, char* request) {
    // Returns first token
    char* token = strtok(request, " ");
    *seq_no = atoi(token);
    
    // Returns second token
    token = strtok(NULL, " ");
    *op_no = atoi(token);
    
    // Returns third token
    token = strtok(NULL, "\0");
    *g_name = token;
    
    return;
}


int getGraphIndex(char* g_name) {
    int i = 1;
    char num[3] = {'\0', '\0', '\0'};
    while(g_name[i] != '.') {
        num[i - 1] = g_name[i];
        i++;
    }
    return atoi(num) - 1;
}


// Function to perform DFS
void* dfsThread(void* data) {
    // Initialising necessary data fo thread
    struct DFSThreadData* threadData = (struct DFSThreadData*)data;
    
    // Marking current node as visited
    *(*(threadData->visited) + threadData->startVertex) = 1;
    
    // Creating array to maintain child threads
    pthread_t threads[MAX_THREADS];
    int threadCount = 0;
    
    // Counting number of nodes not reachable
    int count = 0;
    int n = threadData->nodes;
    struct DFSThreadData newArr[threadData->nodes];
    for (int i = 0; i < n; i++) {
        count++;
        if (threadData->adjMatrix[threadData->startVertex][i] && *(*(threadData->visited) + i) == 0) {
            count--;
            
            // Set up new thread data
            newArr[i].nodes = threadData->nodes;
            newArr[i].startVertex = i;
            newArr[i].visited = threadData->visited;
            newArr[i].countDeep = threadData->countDeep;
            newArr[i].deepestNodes = threadData->deepestNodes;
            for(int j = 0; j< n; j++) {
                for(int k = 0; k < n; k++) {
                    newArr[i].adjMatrix[j][k] = threadData->adjMatrix[j][k];
                }
            }
            
            // Create a new thread for the unvisited node
            pthread_create(&threads[threadCount++], NULL, dfsThread, &newArr[i]);
            
            // Wait for child threads to finish
            pthread_join(threads[threadCount-1], NULL);
        }
    }
    if(count == n) {
        // Storing the deepest vertex to the deepestNodes array
        *(*(threadData->deepestNodes) + (*(threadData->countDeep))) = threadData->startVertex;
        *(threadData->countDeep) = (*(threadData->countDeep)) + 1;
    }
    
    // Wait for all child threads to finish
    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_exit(NULL);
}


// Function to start DFS and send reply back to client
void *multithreadedDFS(void *args) {
    int Sequence_Number;
    int pointless;
    char *Graph_File_Name;
    
    getTokens(&Sequence_Number, &pointless, &Graph_File_Name, (char *)args);
    
    
    int graphIndex = getGraphIndex(Graph_File_Name);
    
    sem_wait(semaphore.mutex[graphIndex]);
    semaphore.counter[graphIndex]++;
    if(semaphore.counter[graphIndex] == 1) {
        sem_wait(semaphore.rw_mutex[graphIndex]);
    }
    sem_post(semaphore.mutex[graphIndex]);
    sem_post(semaphore.in_mutex[graphIndex]);
    
    // Creating file pointer
    FILE *gptr;

    // Opening file
    gptr = fopen(Graph_File_Name,"r");
    if(gptr == NULL) {
      perror("Server: fopen");   
      exit(1);             
    }
    
    // Shared memory segment
    key_t key_shm;
    int shmid;
    char *shm, buff[SHM_BUFFER_SIZE];

    if((key_shm = ftok(FILENAME,Sequence_Number)) == -1)
    {
        perror("Client: ftok\n");
        exit(1);
    }
    if ((shmid = shmget(key_shm,sizeof(char[SHM_BUFFER_SIZE]),SHM_PERMISSIONS | IPC_CREAT)) == -1)
    {
        perror("Client: shmget\n");
        exit(1);
    }

    shm = (char*) shmat(shmid,NULL,0);
    
    // Reading starting vertex from shared memory of client
    char temp1[5];
    strcpy(temp1, shm);
    
    // Set up thread data
    struct DFSThreadData data;

    // Converting to integer
    data.startVertex = atoi(temp1);
    
    // Reading number of nodes from file
    fscanf(gptr, "%d", &data.nodes);
    
    int n = data.nodes;

    // Reading adjacency matrix from file
    for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
            fscanf(gptr, "%d", &data.adjMatrix[i][j]);
        }
    }
    
    // Closing file pointer  
    fclose(gptr);

    sem_wait(semaphore.mutex[graphIndex]);
    semaphore.counter[graphIndex]--;
    if(semaphore.counter[graphIndex] == 0) {
        sem_post(semaphore.rw_mutex[graphIndex]);
    }
    sem_post(semaphore.mutex[graphIndex]);
    
    // DMA for visited array and list of deepest nodes
    int* visited = (int*)malloc(n * sizeof(int));
    int* deepestNodes = (int*)malloc(MAX_NODES * sizeof(int));
    
    // Initialising visited array elements to 0
    for(int i = 0; i < n; i++) {
        visited[i] = 0;
    }
    data.visited = &visited;
    data.deepestNodes = &deepestNodes;
    int countDeep = 0;
    data.countDeep = &countDeep;

    // Create the initial thread
    pthread_t startDFS;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&startDFS, &attr, dfsThread, (void *)&data);
    pthread_join(startDFS, NULL);
    
    Message reply;
    reply.mtext[0] = '\0';

    // Printing list of nodes in reply
    for(int i = 0; i < countDeep; i++) {
        sprintf(reply.mtext + strlen(reply.mtext), "%d ", deepestNodes[i] + 1);
    }

    reply.mtype = Sequence_Number;
    // Reply to client
    if (msgsnd(qd_lb, &reply, sizeof(reply.mtext), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    
    pthread_exit(NULL);   
}


// function to create a queue
// of given capacity.
// It initializes size of queue as 0
struct Queue* createQueue(unsigned capacity)
{
    struct Queue* queue = (struct Queue*)malloc(
        sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
 
    // This is important, see the enqueue
    queue->rear = capacity - 1;
    queue->array = (int*)malloc(
        queue->capacity * sizeof(int));
    return queue;
}
 
 
// Queue is full when size becomes
// equal to the capacity
int isFull(struct Queue* queue)
{
    return (queue->size == queue->capacity);
}
 
 
// Queue is empty when size is 0
int isEmpty(struct Queue* queue)
{
    return (queue->size == 0);
}
 
 
// Function to add an item to the queue.
// It changes rear and size
void enqueue(struct Queue* queue, int item)
{
    if (isFull(queue))
        return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}
 
 
// Function to remove an item from queue.
// It changes front and size
int dequeue(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}
 
 
// Function to get front of queue
int front(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    return queue->array[queue->front];
}
 
 
// Function to get rear of queue
int rear(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    return queue->array[queue->rear];
}


void* enqueueNodes(void* data) {
    
    struct BFSThreadData* threadData = (struct BFSThreadData*)data;
    // Marking current node as visited
    int n = threadData->nodes;
    pthread_mutex_lock (&(threadData->lock));
    for(int i = 0; i < n; i++) {
        if(threadData->adjMatrix[threadData->startVertex][i] == 1 && *(*threadData->visited + i) == 0) {
            enqueue(threadData->queue, i);
            *(*(threadData->visited) + i) = 1;
        }
    }
    pthread_mutex_unlock (&(threadData->lock));
    pthread_exit(NULL);
}


// Function to perform BFS
void* bfsThread(void* data) {
    struct BFSThreadData* threadData = (struct BFSThreadData*)data;
    
    threadData->listOfVertices[*(threadData->listIndex)] = threadData->startVertex;
    *(threadData->listIndex) = *(threadData->listIndex) + 1;
    
    
    enqueue(threadData->queue, threadData->startVertex);
    *(*(threadData->visited) + threadData->startVertex) = 1;
    struct BFSThreadData newArr[threadData->nodes];
    while(!isEmpty(threadData->queue)) {
        int nodeCount = threadData->queue->size;
        // Creating array to maintain child threads
        pthread_t threads[MAX_THREADS];
        int threadCount = 0;
        int t1 = threadData->startVertex;
        

        while(nodeCount > 0) {
            int currNode;
            currNode = dequeue(threadData->queue);
            
            int n = threadData->nodes;
            // struct BFSThreadData newData;
            newArr[currNode].listIndex = threadData->listIndex;
            newArr[currNode].nodes = threadData->nodes;
            newArr[currNode].queue = threadData->queue;
            newArr[currNode].lock = threadData->lock;
            newArr[currNode].startVertex = currNode;
            newArr[currNode].visited = threadData->visited;
            
            for(int i = 0; i< n; i++) {
                for(int j = 0; j < n; j++) {
                    newArr[currNode].adjMatrix[i][j] = threadData->adjMatrix[i][j];
                }
            }
            for(int i = 0; i < n; i++) {
                newArr[currNode].listOfVertices[i] = threadData->listOfVertices[i];
            }
            
            // Create a new thread for every node in same level
            pthread_create(&threads[threadCount++], NULL, enqueueNodes, (void *)&newArr[currNode]);
            
            nodeCount--;    
        }
        
        //threadData->startVertex = t1;
        
        // Wait for all child threads to finish
        for (int i = 0; i < threadCount; i++) {
            pthread_join(threads[i], NULL);
        }
        for(int i = 0; i < (threadData->queue)->size; i++) {
            int t = dequeue(threadData->queue);
            threadData->listOfVertices[*(threadData->listIndex)] = t;
            *(threadData->listIndex) = *(threadData->listIndex) + 1;
            enqueue(threadData->queue, t);
        }
    }
          
    pthread_exit(NULL);
}


// Function to start BFS and send reply back to client
void *multithreadedBFS(void *args) {
    int Sequence_Number;
    int pointless;
    char *Graph_File_Name;
    struct BFSThreadData data;

    getTokens(&Sequence_Number, &pointless, &Graph_File_Name, (char *)args);
    
    int graphIndex = getGraphIndex(Graph_File_Name);

    sem_wait(semaphore.mutex[graphIndex]);
    semaphore.counter[graphIndex]++;
    if(semaphore.counter[graphIndex] == 1) {
        sem_wait(semaphore.rw_mutex[graphIndex]);
    }
    sem_post(semaphore.mutex[graphIndex]);
    sem_post(semaphore.in_mutex[graphIndex]);
    
    // Creating file pointer
    FILE *gptr;

    // Opening file
    gptr = fopen(Graph_File_Name,"r");
    if(gptr == NULL) {
      perror("Server: fopen");   
      exit(1);             
    }
    
    // Shared memory segment
    key_t key_shm;
    int shmid;
    char *shm, buff[SHM_BUFFER_SIZE];

    if((key_shm = ftok(FILENAME,Sequence_Number)) == -1)
    {
        perror("Client: ftok\n");
        exit(1);
    }
    if ((shmid = shmget(key_shm,sizeof(char[SHM_BUFFER_SIZE]),SHM_PERMISSIONS | IPC_CREAT)) == -1)
    {
        perror("Client: shmget\n");
        exit(1);
    }

    shm = (char*) shmat(shmid,NULL,0);
    
    // Reading starting vertex from shared memory of client
    char temp1[5];
    strcpy(temp1, shm);
    
    // Converting to integer
    data.startVertex = atoi(temp1);
    
    // Reading number of nodes from file
    // int nodes;
    fscanf(gptr, "%d", &data.nodes);
       
    int n = data.nodes;
    // Reading adjacency matrix from file
    for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
            fscanf(gptr, "%d", &data.adjMatrix[i][j]);
        }
    }
    // Closing file pointer  
    fclose(gptr);
    
    sem_wait(semaphore.mutex[graphIndex]);
    semaphore.counter[graphIndex]--;
    if(semaphore.counter[graphIndex] == 0) {
        sem_post(semaphore.rw_mutex[graphIndex]);
    }
    sem_post(semaphore.mutex[graphIndex]);
    
    struct Queue* queue = createQueue(QUEUE_SIZE);
    int listIndex = 0;
    //int queueLocks[QUEUE_SIZE];
    int* visited = (int*)malloc(n * sizeof(int));
    // Initialising visited array elements to 0
    for(int i = 0; i < n; i++) {
        visited[i] = 0;
    }

    data.visited = &visited;
 
    for(int i = 0; i < MAX_NODES; i++) {
        data.listOfVertices[i] = -1;
    }
    
    //Initialize Mutex
    if(pthread_mutex_init((pthread_mutex_t *)&(data.lock), NULL) != 0){
        perror("pthread_mutex_init: ");
        exit(1);
    }
    
    data.queue = queue;
    data.listIndex = &listIndex;

    // Create the initial thread
    pthread_t startBFS;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&startBFS, &attr, bfsThread, (void *)&data);
    pthread_join(startBFS, NULL);
    
    if(pthread_mutex_destroy((pthread_mutex_t *)&(data.lock)) != 0){
        perror("thread_mutex_destroy: ");
        exit(1);
    }
    
    Message reply;
    reply.mtext[0] = '\0';

    // Printing list of nodes in reply
    for(int i = 0; i < listIndex; i++) {
        sprintf(reply.mtext + strlen(reply.mtext), "%d ", data.listOfVertices[i] + 1);
    }
    
    reply.mtype = Sequence_Number;
    // Reply to client
    if (msgsnd(qd_lb, &reply, sizeof(reply.mtext), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }    
    
    pthread_exit(NULL);   
}



int main() {
    key_t key_mq;
    Message message;
    long secondary_server_mtype;
    printf("Enter secondary server type: ");
    int temp;
    scanf("%d", &temp);
    
    if(temp == 1) {
        secondary_server_mtype = ODD_SECONDARY_SERVER_MTYPE;
    }
    else if(temp == 2) {
        secondary_server_mtype = EVEN_SECONDARY_SERVER_MTYPE;
    }

    

    // Initialize semaphores for each file
    for (int i = 0; i < NUM_FILES; ++i) {
        semaphore.mutex[i]  = sem_open((const char*)names[0][i], O_EXCL, 0644, 1);
        if (semaphore.mutex[i] == NULL) {
            perror("sem_open in");
            return 1;
        }
        semaphore.rw_mutex[i] = sem_open((const char*)names[1][i], O_EXCL, 0644, 1);
        if (semaphore.rw_mutex[i] == NULL) {
            perror("sem_open out");
            return 1;
        }
        semaphore.in_mutex[i] = sem_open((const char*)names[2][i], O_EXCL, 0644, 1);
        if (semaphore.in_mutex[i] == NULL) {
            perror("sem_open write");
            return 1;
        }
        semaphore.counter[i] = 0;
    }

    // Connect to the load balancer queue
    if ((key_mq = ftok("load_balancer.c", 'A')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    if ((qd_lb = msgget(key_mq, 0644)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
        
    while(1) {
        // Receive a request from load balancer
        if (msgrcv(qd_lb, &message, sizeof(message.mtext), secondary_server_mtype, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        int Sequence_Number;
        int Operation_Number;
        char *Graph_File_Name;
        getTokens(&Sequence_Number, &Operation_Number, &Graph_File_Name, message.mtext);
        
        // Terminate
        if(Sequence_Number == -1) {
            break;
        }
        // Perform DFS
        else if(Operation_Number == 3) {
            char params[25];
            sprintf(params, "%d %d %s", Sequence_Number, 1, Graph_File_Name);
            
            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_create(&tid, &attr, multithreadedDFS, (void *)params);
        }
        // Perform BFS
        else if(Operation_Number == 4) {
            char params[25];
            sprintf(params, "%d %d %s", Sequence_Number, 1, Graph_File_Name);
            
            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_create(&tid, &attr, multithreadedBFS, (void *)params);
        }
    }
    
    // Close semaphores
    for (int i = 0; i < NUM_FILES; ++i) {
        sem_close(semaphore.mutex[i]);
        sem_close(semaphore.rw_mutex[i]);
        sem_close(semaphore.in_mutex[i]);
    }

    return 0;
}