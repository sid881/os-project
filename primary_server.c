#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include<sys/shm.h>
#include <pthread.h>
#include <semaphore.h>


#define LOAD_BALANCER_QUEUE_NAME "/load-balancer-message-queue"
#define MAX_MESSAGES 100
#define MAX_MSG_SIZE 100
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10
#define LOAD_BALANCER_MTYPE 104
#define PRIMARY_SERVER_MTYPE 101
#define FILENAME "./client.c"
#define SHM_BUFFER_SIZE 1024
#define SHM_PERMISSIONS 0666
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


// Global variables
int qd_lb;        // Message queue descriptor 


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


void *addGraph(void *args) {
    int Sequence_Number;
    int pointless;
    char *Graph_File_Name;
    
    getTokens(&Sequence_Number, &pointless, &Graph_File_Name, (char *)args);
    
    int graphIndex = getGraphIndex(Graph_File_Name);

    sem_wait(semaphore.in_mutex[graphIndex]);
    sem_wait(semaphore.rw_mutex[graphIndex]);

    FILE *gptr;
    gptr = fopen(Graph_File_Name,"w");
    if(gptr == NULL) {
      perror("Server: fopen");   
      exit(1);             
    }
    
    // Creating shared memory segment
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
    
    char temp1[3] = {'\0', '\0', '\0'};
    int k = 0;
    while(shm[k] != ',') {
        temp1[k] = *(shm + k);
        k++;
    }
    k++;
    
    int nodes = atoi(temp1);
    fprintf(gptr, "%d\n", nodes);
    
    for(int i = 0; i < nodes; i++) {
        for(int j = 0; j < nodes; j++) {
            fprintf(gptr, "%c ", *(shm + k));
            k += 2;
        }
        fprintf(gptr, "\n");
    }
    
    fclose(gptr);
    
    sem_post(semaphore.rw_mutex[graphIndex]);
    sem_post(semaphore.in_mutex[graphIndex]);

    Message reply;
    sprintf(reply.mtext, "File successfully added");
    
    reply.mtype = Sequence_Number;
    // Reply to client
    if (msgsnd(qd_lb, &reply, sizeof(reply.mtext), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    
    pthread_exit(NULL);    
}


void *modifyGraph(void *args) {
    int Sequence_Number;
    int pointless;
    char *Graph_File_Name;
    
    getTokens(&Sequence_Number, &pointless, &Graph_File_Name, (char *)args);
    
    int graphIndex = getGraphIndex(Graph_File_Name);

    sem_wait(semaphore.in_mutex[graphIndex]);
    sem_wait(semaphore.rw_mutex[graphIndex]);

    FILE *gptr;
    gptr = fopen(Graph_File_Name,"w");
    if(gptr == NULL) {
      perror("Server: fopen");   
      exit(1);             
    }
    
    // Creating shared memory segment
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
    
    // strcpy(newArr, shm);
    char temp1[3] = {'\0', '\0', '\0'};
    int k = 0;
    while(shm[k] != ',') {
        temp1[k] = *(shm + k);
        k++;
    }
    k++;
    
    int nodes = atoi(temp1);
    fprintf(gptr, "%d\n", nodes);
    
    for(int i = 0; i < nodes; i++) {
        for(int j = 0; j < nodes; j++) {
            fprintf(gptr, "%c ", *(shm + k));
            k += 2;
        }
        fprintf(gptr, "\n");
    }
    
    fclose(gptr);
    
    sem_post(semaphore.rw_mutex[graphIndex]);
    sem_post(semaphore.in_mutex[graphIndex]);

    Message reply;
    sprintf(reply.mtext, "File successfully modified");
    
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
    message.mtype = 101;

    // Initialize semaphores for each file
    for (int i = 0; i < NUM_FILES; ++i) {
        semaphore.mutex[i]  = sem_open((const char*)names[0][i], O_EXCL, 0644, 1);
        if (semaphore.mutex[i] == NULL) {
            perror("sem_open in");
            return 1;
        }
        semaphore.in_mutex[i] = sem_open((const char*)names[1][i], O_EXCL, 0644, 1);
        if (semaphore.in_mutex[i] == NULL) {
            perror("sem_open out");
            return 1;
        }
        semaphore.rw_mutex[i] = sem_open((const char*)names[2][i], O_EXCL, 0644, 1);
        if (semaphore.rw_mutex[i] == NULL) {
            perror("sem_open write");
            return 1;
        }
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
        if (msgrcv(qd_lb, &message, sizeof(message.mtext), PRIMARY_SERVER_MTYPE, 0) == -1) {
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
        // Add Graph
        else if(Operation_Number == 1) {
            char params[20];
            sprintf(params, "%d %d %s", Sequence_Number, 1, Graph_File_Name);
            
            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_create(&tid, &attr, addGraph, (void *)params);
            pthread_join(tid, NULL);
        }
        // Modify Graph
        else if(Operation_Number == 2) {
            char params[20];
            sprintf(params, "%d %d %s", Sequence_Number, 1, Graph_File_Name);
            
            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_create(&tid, &attr, modifyGraph, (void *)params);
            pthread_join(tid, NULL);
        }   
    }
    
    // Close semaphores
    for (int i = 0; i < NUM_FILES; ++i) {
        sem_close(semaphore.mutex[i]);
        sem_close(semaphore.in_mutex[i]);
        sem_close(semaphore.rw_mutex[i]);
    }
    
    return 0;
}