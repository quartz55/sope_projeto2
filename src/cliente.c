#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include "log.h"

#define DEBUG 0

void destroyFIFO(char *name);

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage: %s <counter_fifo> <counter_index> <shm>\n", argv[0]);
        exit(1);
    }

    char *COUNTER_FIFO_NAME = argv[1];

    int COUNTER_INDEX = atoi(argv[2]);

    char SHM_NAME[80];
    strcpy(SHM_NAME, argv[3]);

    // Generate FIFO name for client
    char pid_s[80];
    sprintf(pid_s, "%d", getpid());
    char CLIENT_FIFO_NAME[256] = "/tmp/fc_";
    strcat(CLIENT_FIFO_NAME, pid_s);

    // Create private client FIFO
    if (mkfifo(CLIENT_FIFO_NAME, 0660) < 0)
        printf("\t#ERROR# Can't create client FIFO\n");
    else {
        printf("+ Created cliente: %s\n", CLIENT_FIFO_NAME);
    }

    char COUNTER_SEM_NAME[80] = "/";
    int helper = strcspn(COUNTER_FIFO_NAME, "_");
    strncat(COUNTER_SEM_NAME, COUNTER_FIFO_NAME + helper + 1,
            strlen(COUNTER_FIFO_NAME));

    sem_t *counter_sem = sem_open(COUNTER_SEM_NAME, 0, 0600, 0);

    // Write to counter FIFO the client's private FIFO
    int counter_fifo_fd;
    counter_fifo_fd = open(COUNTER_FIFO_NAME, O_WRONLY);
    if (counter_fifo_fd < 0) {
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n",
               COUNTER_FIFO_NAME);
        destroyFIFO(CLIENT_FIFO_NAME);
        exit(1);
    }

    sem_wait(counter_sem);

    // Write to counter FIFO the client fifo name
    logLine(SHM_NAME, 1, COUNTER_INDEX, "pede_atendimento", CLIENT_FIFO_NAME);
    write(counter_fifo_fd, CLIENT_FIFO_NAME, strlen(CLIENT_FIFO_NAME) + 1);

    close(counter_fifo_fd);

    int client_fifo_fd;
    client_fifo_fd = open(CLIENT_FIFO_NAME, O_RDONLY);
    if (client_fifo_fd < 0) {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", CLIENT_FIFO_NAME);
        destroyFIFO(CLIENT_FIFO_NAME);
        exit(1);
    }

    // Only post after we are sure the client is being served
    //     NOTE: The FIFO is blocked until it is open to write
    sem_post(counter_sem);

    // Wait for client to be served
    //    NOTE: Counter will write "fim_atendimento" to FIFO when done
    char fim_atendimento[256];
    read(client_fifo_fd, &fim_atendimento, 256 * sizeof(char));
    if (strcmp("fim_atendimento", fim_atendimento) == 0) {
        printf("- Client served: %s (Message: %s)\n", CLIENT_FIFO_NAME,
               fim_atendimento);
        logLine(SHM_NAME, 1, COUNTER_INDEX, "fim_atendimento",
                CLIENT_FIFO_NAME);
    } else {
        printf("#ERROR# Client '%s' didn't finish being served\n",
               CLIENT_FIFO_NAME);
    }

    close(client_fifo_fd);
    destroyFIFO(CLIENT_FIFO_NAME);

    exit(0);
}

void destroyFIFO(char *name)
{
    if (unlink(name) < 0)
        printf("\t#ERROR# when destroying client FIFO '%s'\n", name);
    else if (DEBUG)
        printf("\t#FIFO# Client '%s' has been destroyed\n", name);
}
