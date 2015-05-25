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
#include <pthread.h>
#include <time.h>

#include "log.h"
#include "memstruct.h"
#include "vector.h"

#define DEBUG 0
#define SHM_SIZE 4096

char SHM_NAME[80];
char COUNTER_FIFO_NAME[80];
int TEMPO_ABERTURA;

typedef struct thread_args
{
    pthread_mutex_t *mutx;
    int startTime;
    char fifo[80];
    int *servedClients;
    int *currClients;
    int *med;
} thread_args;

/* Thread functions */
void *atendimento(void *thread_arg);
void *fifo_thread_function(void *args);

void destroyFIFO(char *name);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <nome_mempartilhada> <tempo_abertura>\n", argv[0]);
        exit(1);
    }

    /* Get process pid */
    pid_t pid = getpid();
    char pid_s[80];
    sprintf(pid_s, "%d", pid); // Convert int to string

    strcpy(SHM_NAME, argv[1]);
    char shm_dir[80] = "/";
    strcat(shm_dir, SHM_NAME);


    TEMPO_ABERTURA = atoi(argv[2]);

    /*
     * Shared memory
     */
    int firstStart = 0;
    int shm_fd = shm_open(shm_dir, O_RDWR, 0600);
    if (shm_fd < 0) // If shared memory doesn't exist, create it
    {
        firstStart = 1;
        shm_fd = shm_open(shm_dir, O_CREAT | O_RDWR, 0600);
        if (shm_fd < 0) {
            printf("#ERROR# Couldn't open shared memory\n");
            exit(2);
        }

        if (ftruncate(shm_fd, SHM_SIZE) < 0) {
            printf("#ERROR# Couldn't allocate space for shared memory\n");
            exit(2);
        }
    }

    memstruct_t *shm;
    shm = (memstruct_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        printf("#ERROR# Couldn't map shared memory\n");
        shm_unlink(shm_dir);
        exit(2);
    }

    if (firstStart) {
        if (pthread_mutex_init(&shm->mutx, NULL) != 0)
            printf("\t#ERROR# Couldn't initialize mutex\n");

        pthread_mutex_lock(&shm->mutx);
        printf("'%s' locked\n", pid_s);
        printf("\n!!!!!!FIRST START!!!!!!!!\n");
        printf("!!!!!!FIRST START!!!!!!!!\n");
        printf("!!!!!!FIRST START!!!!!!!!\n\n");

        clearLog(SHM_NAME);
        logLine(SHM_NAME, 0, 1, "inicia_mempart", "-");

        /* Initialize shared mem data */
        shm->startTime = time(NULL);
        shm->numCounters = 0;

        pthread_mutex_unlock(&shm->mutx);
        printf("'%s' unlocked\n", pid_s);
    }
    /* ------end shared memory-------- */

    /* Generate FIFO name for counter */
    strcpy(COUNTER_FIFO_NAME, "/tmp/fb_");
    strcat(COUNTER_FIFO_NAME, pid_s);


    /* Create counter FIFO */
    if (mkfifo(COUNTER_FIFO_NAME, 0660) < 0) {
        if (errno == EEXIST)
            printf("\t#ERROR# Counter FIFO '%s' already exists\n", COUNTER_FIFO_NAME);
        else
            printf("\t#ERROR# Can't create counter FIFO\n");
    } else {
        printf("+----------------------------------\n");
        printf("| Created counter: %s  -  %ds\n", COUNTER_FIFO_NAME, TEMPO_ABERTURA);
        printf("+----------------------------------\n");
    }


    /* Non busy waiting (opens FIFO in write mode for the specified counter time) */
    int stopCounter = 0;
    pthread_t fifo_thread;
    pthread_create(&fifo_thread, NULL, fifo_thread_function, &stopCounter);

    /* Open counter FIFO to read */
    int counterFifo_fd = open(COUNTER_FIFO_NAME, O_RDONLY);
    if (counterFifo_fd < 0) {
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n", COUNTER_FIFO_NAME);
        destroyFIFO(COUNTER_FIFO_NAME);
        exit(1);
    }

    /* Allocate space for threads */
    pthread_t threads[200];
    thread_args th_args[200];
    int thread_i = 0;

    /* Check for clients until time's up */
    int startTime = time(NULL);

    pthread_mutex_lock(&shm->mutx);
    printf("'%s' locked\n", pid_s);

    /* Initialize counter data */
    counter_t *counter_data = &shm->counters[shm->numCounters];
    counter_data->i = shm->numCounters;
    counter_data->startTime = startTime;
    counter_data->duration = -1;
    strcpy(counter_data->fifo_name, COUNTER_FIFO_NAME);
    counter_data->currClients = 0;
    counter_data->servedClients = 0;
    counter_data->medTime = 0;

    ++shm->numCounters;

    logLine(SHM_NAME, 0, counter_data->i + 1, "cria_linh_mempart", COUNTER_FIFO_NAME);

    memstruct_print(shm);

    pthread_mutex_unlock(&shm->mutx);
    printf("'%s' unlocked\n", pid_s);


    while (1) {
        /* Read FIFO until client arrives (data is written to FIFO) */
        char cli_fifo_buffer[256];
        while ((read(counterFifo_fd, &cli_fifo_buffer, 256 * sizeof(char))) == 0) {
            if (stopCounter) {
                goto CLOSE;
            }
        }

        printf("----- %s ------\n", cli_fifo_buffer);

        /* Create thread to take care of arriving client */
        pthread_mutex_lock(&shm->mutx);
        printf("'%s' locked\n", pid_s);

        th_args[thread_i].mutx = &shm->mutx;
        strcpy(th_args[thread_i].fifo, cli_fifo_buffer);
        th_args[thread_i].startTime = counter_data->startTime;
        th_args[thread_i].servedClients = &counter_data->servedClients;
        th_args[thread_i].currClients = &counter_data->currClients;
        th_args[thread_i].med = &counter_data->medTime;
        pthread_mutex_unlock(&shm->mutx);

        pthread_create(&threads[thread_i], NULL, atendimento,
                       (void *)&th_args[thread_i]);
        ++thread_i;

        printf("'%s' unlocked\n", pid_s);

        continue;

    CLOSE:
        break;
    }

    /* Don't accept more clients */
    pthread_mutex_lock(&shm->mutx);
    printf("'%s' locked\n", pid_s);

    strcpy(counter_data->fifo_name, "-");

    pthread_mutex_unlock(&shm->mutx);
    printf("'%s' unlocked\n", pid_s);

    /* Finish any pending clients before exiting */
    int i;
    for (i = 0; i < thread_i; i++)
        pthread_join(threads[i], NULL);

    /*
     * Cleanup:
     *   - Set duration in shared memory to "-1"
     *   - Close FIFO file descriptor
     *   - Destroy counter FIFO
     *  IF LAST COUNTER
     *   - Print shared memory info
     *   - Destroy mutex
     *   - Clean shared memory
     *   - Unmap shared memory
     *   - Close shared memory
     */

    pthread_mutex_lock(&shm->mutx);
    printf("'%s' locked\n", pid_s);

    counter_data->duration = time(NULL) - counter_data->startTime;
    counter_data->medTime =
        (counter_data->servedClients > 0) ?
        counter_data->duration / counter_data->servedClients : 0;

    memstruct_print(shm);

    printf("+----------------------------------\n");
    printf("| Closing counter: %s\n", COUNTER_FIFO_NAME);
    printf("| Clients served: %d\n", counter_data->servedClients);
    printf("+----------------------------------\n");

    close(counterFifo_fd);
    destroyFIFO(COUNTER_FIFO_NAME);

    logLine(SHM_NAME, 0, counter_data->i + 1, "fecha_balcao", COUNTER_FIFO_NAME);

    pthread_mutex_unlock(&shm->mutx);
    printf("'%s' unlocked\n", pid_s);

    pthread_mutex_lock(&shm->mutx);
    printf("'%s' locked\n", pid_s);

    int lastCounter = 1;
    for (i = 0; i < shm->numCounters; i++) {
        if (shm->counters[i].duration == -1) {
            lastCounter = 0;
            break;
        }
    }

    printf("'%s' unlocked\n", pid_s);
    pthread_mutex_unlock(&shm->mutx);

    if (lastCounter) {
        pthread_mutex_lock(&shm->mutx);
        printf("'%s' locked\n", pid_s);

        logLine(SHM_NAME, 0, counter_data->i + 1, "fecha_loja", COUNTER_FIFO_NAME);

        printf("!!!!!!CLOSING STORE!!!!!!\n");
        printf("!!!!!!CLOSING STORE!!!!!!\n");
        printf("!!!!!!CLOSING STORE!!!!!!\n");

        pthread_mutex_destroy(&shm->mutx);
        printf("Mutex destroyed\n");

        if (munmap(shm, SHM_SIZE) < 0) {
            printf("#ERROR# Couldn't unmap shared memory\n");
        }

        printf("Unmapped shm\n");

        if (shm_unlink(shm_dir) < 0) {
            printf("#ERROR# Couldn't close shared memory\n");
            exit(2);
        }

        printf("Unlinked shm\n");

        exit(0);
    }

    if (munmap(shm, SHM_SIZE) < 0) {
        printf("#ERROR# Couldn't unmap shared memory\n");
    }

    exit(0);
}

void *atendimento(void *thread_arg)
{
    thread_args *args = (thread_args *)thread_arg;

    int cli_fd;
    if ((cli_fd = open(args->fifo, O_WRONLY)) != -1) {
        if (DEBUG)
            printf("\t#FIFO# '%s' openned in WRITEONLY mode\n", args->fifo);
    } else {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", args->fifo);
        return 0;
    }

    pthread_mutex_lock(args->mutx);


    int sleepTime = *args->currClients;
    if (sleepTime > 10) sleepTime = 10;

    char cli_fifo[80];
    strcpy(cli_fifo, args->fifo);

    pthread_mutex_unlock(args->mutx);

    logLine(SHM_NAME, 0, 1, "inicia_atend_cli", cli_fifo);

    printf("+ Client '%s' has arrived\n", cli_fifo);
    printf("* Client '%s' will be served in %d seconds\n", cli_fifo, sleepTime);

    sleep(sleepTime);

    logLine(SHM_NAME, 0, 1, "fim_atend_cli", cli_fifo);
    printf("* Client '%s' has been served\n", cli_fifo);

    while(write(cli_fd, "fim_atendimento", 15) == 0);

    close(cli_fd);

    pthread_mutex_lock(args->mutx);

    ++(*args->servedClients);
    --(*args->currClients);

    *args->med = (time(NULL) - args->startTime) / *args->servedClients;

    pthread_mutex_unlock(args->mutx);

    return (void *)1;
}

void *fifo_thread_function(void *args)
{
    int *ret = (int *)args;
    int counterFifoWrite_fd = open(COUNTER_FIFO_NAME, O_WRONLY);
    if (counterFifoWrite_fd < 0) {
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n", COUNTER_FIFO_NAME);
        destroyFIFO(COUNTER_FIFO_NAME);
        *ret = -1;
        return NULL;
    }

    sleep(TEMPO_ABERTURA);

    close(counterFifoWrite_fd);

    *ret = 1;
    return NULL;
}

void destroyFIFO(char *name)
{
    if (unlink(name) < 0)
        printf("\t#ERROR# when destroying counter FIFO '%s'\n", name);
    else if (DEBUG)
        printf("\t#FIFO# Counter '%s' has been destroyed\n", name);
}
