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
#define SHM_SIZE 4096

char SHM_NAME[80];
char COUNTER_FIFO_NAME[80];
int TEMPO_ABERTURA;

typedef struct thread
{
    pthread_t tid;
    struct th_args
    {
        pthread_mutex_t *mutx;
        char fifo[80];
        counter_t *counter;
    }args;

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

    /* Get process pid */
    pid_t pid = getpid();
    char pid_s[80];
    sprintf(pid_s, "%d", pid); // Convert int to string

    strcpy(SHM_NAME, argv[1]);
    char shm_dir[80] = "/";
    strcat(shm_dir, SHM_NAME);

    char SEM_NAME[80];
    strcpy(SEM_NAME, pid_s);
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0600, 0);

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
        printf("\n!!!!!!CREATING STORE!!!!!!!!\n");
        printf("!!!!!!CREATING STORE!!!!!!!!\n");
        printf("!!!!!!CREATING STORE!!!!!!!!\n\n");

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
    Vector_t *threads = Vector_new();

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

    /* Counter is ready to start reading client requests */
    sem_post(sem);

    while (1) {
        /* Read FIFO until client arrives (data is written to FIFO) */
        char cli_fifo_buffer[256];
        while ((read(counterFifo_fd, &cli_fifo_buffer, 256 * sizeof(char))) == 0) {
            if (stopCounter) {
                goto CLOSE;
            }
        }

        pthread_mutex_lock(&shm->mutx);
        printf("'%s' locked\n", pid_s);

        printf("----- %s ------\n", cli_fifo_buffer);

        /* Create thread to take care of arriving client */

        thread *t = (thread *)malloc(sizeof(thread));
        t->args.mutx = &shm->mutx;
        t->args.counter = counter_data;
        strcpy(t->args.fifo, cli_fifo_buffer);
        Vector_push(threads, t);

        pthread_mutex_unlock(&shm->mutx);
        printf("'%s' unlocked\n", pid_s);

        pthread_create(&t->tid, NULL, atendimento,
                       (void *)&t->args);

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
    for (i = 0; i < Vector_size(threads); i++)
        pthread_join(((thread *)Vector_get(threads, i))->tid, NULL);

    /*
     * Cleanup:
     *   - Set duration in shared memory to "-1"
     *   - Close FIFO file descriptor
     *   - Destroy counter FIFO
     *   - Close and unlink semaphore
     *   - Free memory used by threads
     *  IF LAST COUNTER
     *   - Destroy mutex
     *   - Clean shared memory
     *   - Unmap shared memory
     *   - Close shared memory
     */

    pthread_mutex_lock(&shm->mutx);
    printf("'%s' locked\n", pid_s);

    counter_data->duration = time(NULL) - counter_data->startTime;
    counter_data->medTime /= counter_data->servedClients;

    memstruct_print(shm);

    printf("+----------------------------------\n");
    printf("| Closing counter: %s\n", COUNTER_FIFO_NAME);
    printf("| Clients served: %d\n", counter_data->servedClients);
    printf("+----------------------------------\n");

    close(counterFifo_fd);
    destroyFIFO(COUNTER_FIFO_NAME);

    sem_close(sem);
    sem_unlink(SEM_NAME);

    logLine(SHM_NAME, 0, counter_data->i + 1, "fecha_balcao", COUNTER_FIFO_NAME);

    Vector_destroy(threads);

    pthread_mutex_unlock(&shm->mutx);
    printf("'%s' unlocked\n", pid_s);

    /*
     * --- If last counter ---
     */

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
    struct th_args *args = (struct th_args *)thread_arg;

    int cli_fd;
    if ((cli_fd = open(args->fifo, O_WRONLY)) != -1) {
        if (DEBUG)
            printf("\t#FIFO# '%s' openned in WRITEONLY mode\n", args->fifo);
    } else {
        printf("\t#ERROR# Couldn't open client FIFO '%s'\n", args->fifo);
        return 0;
    }

    pthread_mutex_lock(args->mutx);

    ++args->counter->currClients;

    int sleepTime = args->counter->currClients;
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

    ++args->counter->servedClients;
    --args->counter->currClients;

    args->counter->medTime += sleepTime;

    pthread_mutex_unlock(args->mutx);

    return (void *)1;

    /* thread_args *args = (thread_args *)thread_arg; */

    /* struct th_args *args = (struct th_args *)thread_arg; */

    /* int cli_fd; */
    /* if ((cli_fd = open(args->fifo, O_WRONLY)) != -1) { */
    /*     if (DEBUG) */
    /*         printf("\t#FIFO# '%s' openned in WRITEONLY mode\n", args->fifo); */
    /* } else { */
    /*     printf("\t#ERROR# Couldn't open client FIFO '%s'\n", args->fifo); */
    /*     return 0; */
    /* } */

    /* pthread_mutex_lock(args->mutx); */

    /* ++(*args->currClients); */

    /* int sleepTime = *args->currClients; */
    /* if (sleepTime > 10) sleepTime = 10; */

    /* char cli_fifo[80]; */
    /* strcpy(cli_fifo, args->fifo); */

    /* pthread_mutex_unlock(args->mutx); */

    /* logLine(SHM_NAME, 0, 1, "inicia_atend_cli", cli_fifo); */

    /* printf("+ Client '%s' has arrived\n", cli_fifo); */
    /* printf("* Client '%s' will be served in %d seconds\n", cli_fifo, sleepTime); */

    /* sleep(sleepTime); */

    /* logLine(SHM_NAME, 0, 1, "fim_atend_cli", cli_fifo); */
    /* printf("* Client '%s' has been served\n", cli_fifo); */

    /* while(write(cli_fd, "fim_atendimento", 15) == 0); */

    /* close(cli_fd); */

    /* pthread_mutex_lock(args->mutx); */

    /* ++(*args->servedClients); */
    /* --(*args->currClients); */

    /* int prevMed = *args->med; */
    /* int prevClis = (*args->servedClients)-1; */
    /* int newMed = ((prevMed * prevClis) + sleepTime) / (*args->servedClients); */

    /* *args->med += sleepTime; */

    /* pthread_mutex_unlock(args->mutx); */

    /* return (void *)1; */
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
