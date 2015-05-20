#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "log.h"

#define DEBUG 0

void destroyFIFO(char *name);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: cli_03 <b_fifo>\n");
        exit(1);
    }

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
        if(DEBUG) printf("\t#FIFO# Client '%s' sucessfully created\n", FIFO_name);

        printf("+----------------------------------\n");
        printf("| Created cliente: %s\n", FIFO_name);
        printf("+----------------------------------\n");
    }
    /* ---------------------------- */


    printf(". Looking for available counter...\n");

    char *B_FIFO = argv[1];
    printf("Found '%s' counter\n", B_FIFO);

    /* Write to counter FIFO the client's private FIFO */
    int fd;
    fd = open(B_FIFO, O_WRONLY);
    if (fd != -1) {
        if(DEBUG)
            printf("\t#FIFO# Client '%s' openned in WRITEONLY mode\n", B_FIFO);
    }
    else{
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n", B_FIFO);
        destroyFIFO(FIFO_name);
        exit(1);
    }

    myLog("teste", 1, 1, "pede_atendimento", FIFO_name);
    /* Write to counter FIFO the client fifo name */
    write(fd, FIFO_name, strlen(FIFO_name) + 1);

    close(fd);

    int cli_fd;
    if ((cli_fd = open(FIFO_name, O_RDONLY)) != -1)
    {
        if(DEBUG) printf("\t#FIFO# Client '%s' openned in READONLY mode\n", FIFO_name);
    }
    else
    {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", FIFO_name);
        destroyFIFO(FIFO_name);
        exit(1);
    }

    /* Wait for counter to finish */
    char fim_atendimento[256];
    while ((read(cli_fd, &fim_atendimento, 256*sizeof(char))) == 0);
    if(strcmp("fim_atendimento", fim_atendimento) == 0){
        printf("%s is finished (%s)\n", FIFO_name, fim_atendimento);
        myLog("teste", 1, 1, "fim_atendimento", FIFO_name);
    }
    else
    {
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
