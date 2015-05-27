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
#include <semaphore.h>

#include "log.h"
#include "memstruct.h"
#include "vector.h"

#define DEBUG 0

char SHM_NAME[80];
char SHM_DIR[80] = "/";
char COUNTER_FIFO_NAME[80];
int TEMPO_ABERTURA;

typedef struct thread
{
    pthread_t tid;
    struct th_args
    {
        sem_t *sem;
        char fifo[80];
        counter_t *counter;
    } args;

} thread;

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

    // Get process pid
    char pid_s[80];
    sprintf(pid_s, "%d", getpid()); // Convert int to string

    TEMPO_ABERTURA = atoi(argv[2]);

    char COUNTER_SEM_NAME[80] = "/";
    strcat(COUNTER_SEM_NAME, pid_s);
    sem_t *counter_sem = sem_open(COUNTER_SEM_NAME, O_CREAT, 0600, 0);

    /*
     * Shared memory
     */

    // SHM strings
    strcpy(SHM_NAME, argv[1]);
    strcat(SHM_DIR, SHM_NAME);

    // SHM semaphore
    sem_t *shm_sem = sem_open(SHM_DIR, 0, 0600, 0);
    if (shm_sem == SEM_FAILED) {
        shm_sem = sem_open(SHM_DIR, O_CREAT, 0600, 0);
        printf("Creating SHM semaphore: %s\n", SHM_DIR);
    } else {
        sem_wait(shm_sem);
    }

    int firstStart = 0;
    int shm_fd = shm_open(SHM_DIR, O_RDWR, 0600);
    if (shm_fd < 0) { // If shared memory doesn't exist, create it
        firstStart = 1;
        shm_fd = shm_open(SHM_DIR, O_CREAT | O_RDWR, 0600);
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
    shm = (memstruct_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                              shm_fd, 0);
    if (shm == MAP_FAILED) {
        printf("#ERROR# Couldn't map shared memory\n");
        shm_unlink(SHM_DIR);
        exit(2);
    }

    if (firstStart) {
        printf("\n!!!!!!CREATING STORE!!!!!!!!\n");
        printf("!!!!!!CREATING STORE!!!!!!!!\n");
        printf("!!!!!!CREATING STORE!!!!!!!!\n\n");

        clearLog(SHM_NAME);
        logLine(SHM_NAME, 0, 1, "inicia_mempart", "-");

        /* Initialize shared mem data */
        shm->startTime = time(NULL);
        shm->numCounters = 0;
        shm->activeCounters = 0;
    }
    /* ------end shared memory-------- */

    // Generate FIFO name for counter
    strcpy(COUNTER_FIFO_NAME, "/tmp/fb_");
    strcat(COUNTER_FIFO_NAME, pid_s);

    // Create counter FIFO
    if (mkfifo(COUNTER_FIFO_NAME, 0660) < 0) {
        printf("\t#ERROR# Can't create counter FIFO\n");
    } else {
        printf("+ Created counter: %s  -  %ds\n", COUNTER_FIFO_NAME,
               TEMPO_ABERTURA);
    }

    // Non busy waiting (opens FIFO in write mode for the specified counter
    // time)
    int stopCounter = 0;
    pthread_t fifo_thread;
    pthread_create(&fifo_thread, NULL, fifo_thread_function, &stopCounter);

    // Open counter FIFO to read
    int counterFifo_fd = open(COUNTER_FIFO_NAME, O_RDONLY);
    if (counterFifo_fd < 0) {
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n",
               COUNTER_FIFO_NAME);
        destroyFIFO(COUNTER_FIFO_NAME);
        exit(1);
    }

    // Allocate space for threads
    Vector_t *threads = Vector_new();

    // Check for clients until time's up
    int startTime = time(NULL);

    // Initialize counter data
    counter_t *counter_data = &shm->counters[shm->numCounters];
    counter_data->i = shm->numCounters;
    counter_data->startTime = startTime;
    counter_data->duration = -1;
    strcpy(counter_data->fifo_name, COUNTER_FIFO_NAME);
    counter_data->currClients = 0;
    counter_data->servedClients = 0;
    counter_data->medTime = 0;

    ++shm->numCounters;
    ++shm->activeCounters;

    logLine(SHM_NAME, 0, counter_data->i + 1, "cria_linh_mempart",
            COUNTER_FIFO_NAME);

    memstruct_print(shm);

    // Counter is ready to start reading client requests
    sem_post(shm_sem);
    sem_post(counter_sem);

    while (1) {
        // Read FIFO until client arrives (data is written to FIFO)
        char cli_fifo_buffer[256];
        while ((read(counterFifo_fd, &cli_fifo_buffer, 256 * sizeof(char))) ==
               0) {
            if (stopCounter) {
                goto CLOSE;
            }
        }

        sem_wait(shm_sem);

        // Create thread to serve arriving client
        thread *t = (thread *)malloc(sizeof(thread));
        t->args.sem = shm_sem;
        t->args.counter = counter_data;
        strcpy(t->args.fifo, cli_fifo_buffer);
        Vector_push(threads, t);

        pthread_create(&t->tid, NULL, atendimento, (void *)&t->args);

        sem_post(shm_sem);

        continue;

    CLOSE:
        break;
    }

    sem_wait(shm_sem);

    // Don't accept more clients
    strcpy(counter_data->fifo_name, "-");

    sem_post(shm_sem);

    // Finish any pending clients before exiting
    int i;
    for (i = 0; i < Vector_size(threads); i++)
        pthread_join(((thread *)Vector_get(threads, i))->tid, NULL);

    /*
     * Cleanup:
     *   - Set duration in shared memory to "-1"
     *   - Close FIFO file descriptor
     *   - Destroy counter FIFO
     *   - Close and unlink semaphore
     *   - Free memory used by threads
     * -- IF LAST COUNTER --
     *   - Destroy mutex
     *   - Clean shared memory
     *   - Unmap shared memory
     *   - Close shared memory
     */
    sem_wait(shm_sem);

    counter_data->duration = time(NULL) - counter_data->startTime;
    counter_data->medTime =
        counter_data->medTime /
        (counter_data->servedClients ? counter_data->servedClients : 1);

    --shm->activeCounters;

    memstruct_print(shm);

    printf("- Closing counter: %s\n", COUNTER_FIFO_NAME);

    close(counterFifo_fd);
    destroyFIFO(COUNTER_FIFO_NAME);

    sem_close(counter_sem);
    sem_unlink(COUNTER_SEM_NAME);

    logLine(SHM_NAME, 0, counter_data->i + 1, "fecha_balcao",
            COUNTER_FIFO_NAME);

    Vector_destroy(threads);

    /*
     * --- If last counter ---
     */
    if (shm->activeCounters <= 0) {
        logLine(SHM_NAME, 0, counter_data->i + 1, "fecha_loja",
                COUNTER_FIFO_NAME);

        printf("\n!!!!!!CLOSING STORE!!!!!!\n");
        printf("!!!!!!CLOSING STORE!!!!!!\n");
        printf("!!!!!!CLOSING STORE!!!!!!\n\n");

        sem_unlink(SHM_NAME);

        if (munmap(shm, SHM_SIZE) < 0) {
            printf("#ERROR# Couldn't unmap shared memory\n");
        }

        printf("Unmapped shm\n");

        if (shm_unlink(SHM_DIR) < 0) {
            printf("#ERROR# Couldn't close shared memory\n");
            exit(2);
        }

        printf("Unlinked shm\n");

        exit(0);
    }

    sem_post(shm_sem);

    if (munmap(shm, SHM_SIZE) < 0) {
        printf("#ERROR# Couldn't unmap shared memory\n");
    }

    exit(0);
}

void *atendimento(void *thread_arg)
{
    struct th_args *args = (struct th_args *)thread_arg;

    char cli_fifo[80];
    strcpy(cli_fifo, args->fifo);

    pid_t pid = getpid();

    int cli_fd;
    if ((cli_fd = open(cli_fifo, O_WRONLY)) < 0) {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", cli_fifo);
        return 0;
    }

    sem_wait(args->sem);

    ++args->counter->currClients;

    int sleepTime = args->counter->currClients;
    if (sleepTime > 10)
        sleepTime = 10;

    logLine(SHM_NAME, 0, 1, "inicia_atend_cli", cli_fifo);

    printf("[%d] + Client '%s' will be served in %d seconds\n", pid, cli_fifo,
           sleepTime);

    sem_post(args->sem);

    sleep(sleepTime);

    sem_wait(args->sem);

    logLine(SHM_NAME, 0, 1, "fim_atend_cli", cli_fifo);
    printf("[%d] - Client '%s' has been served\n", pid, cli_fifo);

    while (write(cli_fd, "fim_atendimento", 15) == 0);

    close(cli_fd);

    ++args->counter->servedClients;
    --args->counter->currClients;
    args->counter->medTime += sleepTime;

    sem_post(args->sem);

    return (void *)1;
}

void *fifo_thread_function(void *args)
{
    int *ret = (int *)args;
    int counterFifoWrite_fd = open(COUNTER_FIFO_NAME, O_WRONLY);
    if (counterFifoWrite_fd < 0) {
        printf("\t#ERROR# Couldn't open counter FIFO '%s'\n",
               COUNTER_FIFO_NAME);
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
