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
#include <semaphore.h>


#define LOAD_BALANCER_QUEUE_NAME "/load-balancer-message-queue"
#define QUEUE_PERMISSIONS 0644
#define MAX_MESSAGES 100
#define MAX_MSG_SIZE 100
#define LOAD_BALANCER_MTYPE 104
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10
#define PRIMARY_SERVER_MTYPE 101
#define EVEN_SECONDARY_SERVER_MTYPE 102
#define ODD_SECONDARY_SERVER_MTYPE 103
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
    sem_t* in_semaphores[NUM_FILES];  // Semaphores for each file
    sem_t* out_semaphores[NUM_FILES];  // Semaphores for each file
    sem_t* write_semaphores[NUM_FILES];  // Semaphores for each file
} Semaphore;


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


int main() {
    key_t key_mq;
    int qd_lb;
    Message message;
    Semaphore semaphore;

    // Initialize semaphores for each file
    for (int i = 0; i < NUM_FILES; ++i) {
        semaphore.in_semaphores[i]  = sem_open((const char*)names[0][i], O_CREAT | O_EXCL, 0644, 1);
        if (semaphore.in_semaphores[i] == NULL) {
            perror("sem_open in");
            return 1;
        }
        semaphore.out_semaphores[i] = sem_open((const char*)names[1][i], O_CREAT | O_EXCL, 0644, 1);
        if (semaphore.out_semaphores[i] == NULL) {
            perror("sem_open out");
            return 1;
        }
        semaphore.write_semaphores[i] = sem_open((const char*)names[2][i], O_CREAT | O_EXCL, 0644, 1);
        if (semaphore.write_semaphores[i] == NULL) {
            perror("sem_open write");
            return 1;
        }
    }

    // Generate a unique key for the message queue
    if ((key_mq = ftok("load_balancer.c", 'A')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    
    // Create a message queue
    if ((qd_lb = msgget(key_mq, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
         
    while(1) {
        message.mtype = LOAD_BALANCER_MTYPE;

        // Receive a request from a client
        if (msgrcv(qd_lb, &message, sizeof(message.mtext), LOAD_BALANCER_MTYPE, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        int Sequence_Number;
        int Operation_Number;
        char *Graph_File_Name;
        
        getTokens(&Sequence_Number, &Operation_Number, &Graph_File_Name, message.mtext);
        
        // This statement is necessary as using tokenizer changes mtext
        sprintf(message.mtext, "%d %d %s", Sequence_Number, Operation_Number, Graph_File_Name);
        
        if(Sequence_Number == -1) {
            message.mtype = PRIMARY_SERVER_MTYPE;
            // Primary server
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            // Even secondary server
            message.mtype = EVEN_SECONDARY_SERVER_MTYPE;
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            // Odd secondary server
            message.mtype = ODD_SECONDARY_SERVER_MTYPE;
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            break;
        }
        else {
            if(Operation_Number == 1 || Operation_Number == 2) {
                message.mtype = PRIMARY_SERVER_MTYPE;


                // Primary server
                if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                    perror("msgsnd");
                    exit(EXIT_FAILURE);
                }
            }
            else if(Operation_Number == 3 || Operation_Number == 4) {
                // Odd secondary server
                if(Sequence_Number % 2 == 1) {
                    message.mtype = ODD_SECONDARY_SERVER_MTYPE;
                    if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                        perror("msgsnd");
                        exit(EXIT_FAILURE);
                    }
                }
                // Even secondary server
                else if(Sequence_Number % 2 == 0) {
                    message.mtype = EVEN_SECONDARY_SERVER_MTYPE;
                    if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                        perror("msgsnd");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }   
    }
    
    sleep(5);

    // Close semaphores
    for (int i = 0; i < NUM_FILES; ++i) {
        sem_close(semaphore.in_semaphores[i]);
        sem_close(semaphore.out_semaphores[i]);
        sem_close(semaphore.write_semaphores[i]);
    }

    // Unlink semaphores
    for (int i = 0; i < NUM_FILES; ++i) {
        sem_unlink((const char*)names[0][i]);
        sem_unlink((const char*)names[1][i]);
        sem_unlink((const char*)names[2][i]);
    }

    // Remove the message queue
    if (msgctl(qd_lb, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }
    
    printf("Server terminated\n");
    
    return 0;
}