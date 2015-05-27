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
        printf("Usage: %s <counter_fifo> <counter_number> <shm>\n", argv[0]);
        exit(1);
    }

    char *B_FIFO = argv[1];

    int counterNumber = atoi(argv[2]);

    char SHM_NAME[80];
    strcpy(SHM_NAME, argv[3]);

    /* Generate FIFO name for client */
    pid_t pid;
    pid = getpid();
    char pid_s[80];
    sprintf(pid_s, "%d", pid);

    char FIFO_name[256] = "/tmp/fc_";
    strcat(FIFO_name, pid_s);

    /* Create private client FIFO */
    if (mkfifo(FIFO_name, 0660) < 0)
        if (errno == EEXIST)
            printf("\t#FIFO# Client '%s' already exists\n", FIFO_name);
        else
            printf("\t#ERROR# Can't create client FIFO\n");
    else
    {
        printf("+ Created cliente: %s\n", FIFO_name);
    }
    /* ---------------------------- */



    char SEM_NAME[80];
    int index = strcspn(B_FIFO, "_");
    strncpy(SEM_NAME, B_FIFO+index+1, strlen(B_FIFO));

    sem_t *sem = sem_open(SEM_NAME, 0, 0600, 0);

    /* Write to counter FIFO the client's private FIFO */
    int fd;
    fd = open(B_FIFO, O_WRONLY);
    if (fd < 0){
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n", B_FIFO);
        destroyFIFO(FIFO_name);
        exit(1);
    }

    sem_wait(sem);

    logLine(SHM_NAME, 1, counterNumber, "pede_atendimento", FIFO_name);
    /* Write to counter FIFO the client fifo name */
    write(fd, FIFO_name, strlen(FIFO_name) + 1);
    close(fd);

    int cli_fd;
    cli_fd = open(FIFO_name, O_RDONLY);
    if (cli_fd < 0) {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", FIFO_name);
        destroyFIFO(FIFO_name);
        exit(1);
    }

    // Only post after we are sure the client is being served
    sem_post(sem);

    /* Wait for counter to finish */
    char fim_atendimento[256];
    while ((read(cli_fd, &fim_atendimento, 256*sizeof(char))) == 0) {
        printf("%s\n", FIFO_name);
    }
    if(strcmp("fim_atendimento", fim_atendimento) == 0){
        printf("- %s is finished (%s)\n", FIFO_name, fim_atendimento);
        logLine(SHM_NAME, 1, counterNumber, "fim_atendimento", FIFO_name);
    }
    else {
        printf("#ERROR# Client '%s' didn't finish being served\n", FIFO_name);
    }

    close(cli_fd);
    destroyFIFO(FIFO_name);

    exit(0);
}

void destroyFIFO(char *name)
{
    if (unlink(name) < 0)
        printf("\t#ERROR# when destroying client FIFO '%s'\n", name);
    else
        if(DEBUG) printf("\t#FIFO# Client '%s' has been destroyed\n", name);
}
