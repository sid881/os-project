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

#define LOAD_BALANCER_QUEUE_NAME "/load-balancer-message-queue"
#define MAX_MESSAGES 100
#define MAX_MSG_SIZE 100
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10
#define LOAD_BALANCER_MTYPE 104
#define FILENAME "./client.c"
#define SHM_BUFFER_SIZE 1024
#define SHM_PERMISSIONS 0666


// Structure for the message
typedef struct Message {
    long mtype; // Message type (must be greater than 0)
    char mtext[MAX_MSG_SIZE]; // Message data
} Message;


int main() {
    key_t key_mq;
    int qd_lb;
    Message message;
    
    // Generate a unique key for the message queue
    if ((key_mq = ftok("load_balancer.c", 'A')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    if ((qd_lb = msgget(key_mq, 0644)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Infinite loop
    while(1) {
        // Display the menu to the user
        printf("Menu:\n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph of the database\n");
        printf("4. Perform BFS on an existing graph of the database\n");
        
        int Sequence_Number;
        int Operation_Number;
        char Graph_File_Name[10];
        
        printf("Enter Sequence Number: ");
        scanf("%d", &Sequence_Number);
        
        printf("Enter Operation Number: ");
        scanf("%d", &Operation_Number);
        
        printf("Enter Graph File Name: ");
        scanf(" %s", Graph_File_Name);
        
    
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
       
        
        if(Operation_Number == 1 || Operation_Number == 2) {
            int nodes;
            printf("Enter number of nodes of the graph: ");
            scanf("%d", &nodes);
            
            int adjMatrix[nodes][nodes];
            printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters: \n");
            for(int i = 0; i < nodes; i++) {
                for(int j = 0; j < nodes; j++) {
                    scanf("%d", &adjMatrix[i][j]);
                }
            }
            
            // Linerarising row by row 2-D array. 
            // Format: Nodes,adj[0][0],adj[0][1],...,adj[1][0],adj[1][1],...,adj[nodes-1][nodes-1]
            // NOTE: commas are seperate characters to be counted
            char newArr[2 * nodes * nodes + 5];
            sprintf(newArr, "%d,", nodes);
            int k = strlen(newArr);
            for(int i = 0; i < nodes; i++) {
                for(int j = 0; j < nodes; j++) {
                    newArr[k] = (char)(adjMatrix[i][j] + 48);
                    k++;
                    newArr[k] = ',';
                    k++;
                }
            }
            newArr[k] = '\0';
            
            // Writing to shared memory
            strcpy(shm, newArr);
            
            message.mtype = LOAD_BALANCER_MTYPE;
            // Send request to server
            sprintf(message.mtext, "%d %d %s", Sequence_Number, Operation_Number, Graph_File_Name);
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }         
        }
        
        else if(Operation_Number == 3 || Operation_Number == 4) {
            int startingVertex;
            printf("Enter starting vertex: ");
            scanf("%d", &startingVertex);
            startingVertex -= 1;
            
            char newArr[5];
            sprintf(newArr, "%d", startingVertex);
            
            // Writing to shared memory
            strcpy(shm, newArr);
            
            message.mtype = LOAD_BALANCER_MTYPE;
            // Send request to server
            sprintf(message.mtext, "%d %d %s", Sequence_Number, Operation_Number, Graph_File_Name);
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
        }
        
        // Wait for reply from server
        // Maximum reply size, a list of 30 nodes, can be around 80 (counting the spaces in between) for case 3 and 4, hence MAX_MSG_SIZE = 100
        if (msgrcv(qd_lb, &message, sizeof(message.mtext), (long)Sequence_Number, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        // Display the output to the user
        printf("%s\n\n", message.mtext);
        
        // Deleting the shared memory segment
        if(shmdt(shm) == -1)
        {
            perror("Client: shmdt\n");
            exit(1);
        }

        if(shmctl(shmid,IPC_RMID,0) == -1)
        {
            perror("Client: shmctl\n");
            exit(1);
        }
    }
    
    
    
    printf("\nTerminated client session\n");
    exit(0);
}
