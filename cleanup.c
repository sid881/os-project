#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

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


int main(v) {
    key_t key_mq;
    int qd_lb;
    Message message;
    message.mtype = LOAD_BALANCER_MTYPE;

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
        // Ask the user to terminate server
        printf("\nWant to terminate the application? Press Y (Yes) or N (No)\n");

        char choice;
        
        // Get user choice
        scanf(" %c", &choice);
        
        // Inform server to terminate
        if(choice == 'Y' || choice == 'y') {
            sprintf(message.mtext, "%s %s %s", "-1", "4", "Terminate");
            if (msgsnd(qd_lb, &message, sizeof(message.mtext), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
            break;
        }
        // Continue in infinite loop
        else if (choice == 'N' || choice == 'n') {
            continue;
        }
        else {
            printf("\nEnter a valid option.\n");
            continue;
        }
    }

    printf("\nTerminating server\n");
    return 0;
}