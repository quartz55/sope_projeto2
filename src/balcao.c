#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "log.h"

#define DEBUG 0

struct thread_data
{
    char *b_fifo;
    char *fifo;
    int *served;
};

int num_clis = 0;

void *atendimento(void *thread_arg);
void destroyFIFO(char *name);

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s <tempo_abertura>\n", argv[0]);
        exit(1);
    }

    setbuf(stdout, NULL);

    int tempo_abertura = atoi(argv[1]);

    /* Get process pid */
    pid_t pid = getpid();
    char pid_s[80];
    sprintf(pid_s, "%d", pid); // Convert int to string

    /* Generate FIFO name for counter */
    char FIFO_name[256] = "/tmp/fb_";
    strcat(FIFO_name, pid_s);

    /* Create counter FIFO */
    if (mkfifo(FIFO_name, 0660) < 0)
    {
        if (errno == EEXIST)
            printf("\t#ERROR# Counter FIFO '%s' already exists\n", FIFO_name);
        else
            printf("\t#ERROR# Can't create counter FIFO\n");
    }
    else
    {
        if(DEBUG) printf("\t#FIFO# Counter '%s' sucessfully created\n", FIFO_name);

        printf("+----------------------------------\n");
        printf("| Created counter: %s  -  %ds\n", FIFO_name, tempo_abertura);
        printf("+----------------------------------\n");
    }

    /* Open counter FIFO to read */
    int fd = open(FIFO_name, O_RDONLY);
    if (fd != -1){
        if(DEBUG) printf("\t#FIFO# '%s' openned in READONLY mode\n", FIFO_name);
    }
    else{
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n", FIFO_name);

        destroyFIFO(FIFO_name);
        exit(1);
    }

    /* Allocate space for threads */
    pthread_t threads[50];
    struct thread_data th_data_array[50];
    int thread_i = 0;
    int clientsServed = 0;

    /* Check for clients until time's up */
    int startTime = time(NULL);
    int endTime = startTime+tempo_abertura;

    printf("Current time: %d  |  End time: %d\n", startTime, endTime);
    while(time(NULL) <= endTime)
    {
        /* Read FIFO until client arrives */
        printf(". Waiting for clients...\n");

        int interrupt = 0;
        char cli_fifo_buffer[256];
        while ((read(fd, &cli_fifo_buffer, 256*sizeof(char))) == 0){
            if(time(NULL) > endTime){
                interrupt = 1;
                break;
            }
        }
        if(interrupt) break;

        printf("+ Client %s has arrived\n", cli_fifo_buffer);
        ++num_clis;

        myLog("teste", 0, 1, "inicia_atend_cli", cli_fifo_buffer);

        /* Create thread to take care of arriving client */
        th_data_array[thread_i].b_fifo = FIFO_name;
        th_data_array[thread_i].fifo = cli_fifo_buffer;
        th_data_array[thread_i].served = &clientsServed;
        pthread_create(&threads[thread_i], NULL, atendimento, (void *)&th_data_array[thread_i]);
        ++thread_i;
    }

    /* Finish any pending clients before exiting */
    int i;
    for(i = 0; i < thread_i; i++)
        pthread_join(threads[i], NULL);


    /*
     * Cleanup:
     *   - Close FIFO file descriptor
     *   - Destroy counter FIFO
     */
    printf("+----------------------------------\n");
    printf("| Closing counter: %s\n", FIFO_name);
    printf("| Clients served: %d\n", clientsServed);
    printf("+----------------------------------\n");

    close(fd);
    destroyFIFO(FIFO_name);

    myLog("teste", 0, 1, "fecha_balcao", FIFO_name);

    exit(0);
}
void *atendimento(void *thread_arg)
{
    struct thread_data *my_data;
    my_data = (struct thread_data *) thread_arg;

    int cli_fd;
    if ((cli_fd = open(my_data->fifo, O_WRONLY)) != -1){
        if(DEBUG) printf("\t#FIFO# '%s' openned in WRITEONLY mode\n", my_data->fifo);
    }
    else {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", my_data->fifo);
        --num_clis;
        return 0;
    }

    printf("* Client '%s' will take %d seconds to finish\n", my_data->fifo, num_clis);
    sleep(num_clis);

    myLog("teste", 0, 1, "fim_atend_cli", my_data->fifo);
    write(cli_fd, "fim_atendimento", 15);
    close(cli_fd);

    ++(*my_data->served);
    --num_clis;
    return 1;
}

void destroyFIFO(char *name)
{
    if (unlink(name) < 0)
        printf("\t#ERROR# when destroying counter FIFO '%s'\n", name);
    else
        if(DEBUG) printf("\t#FIFO# Counter '%s' has been destroyed\n", name);
}
