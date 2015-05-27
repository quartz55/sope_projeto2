#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/wait.h>
#include <dirent.h>
#include <libgen.h>
#include <semaphore.h>

#include "memstruct.h"
#include "vector.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <nome_mempartilhada> <num_clientes>\n", argv[0]);
        exit(3);
    }

    // Get path to 'client'
    char CLIENT_PATH[256];
    realpath(argv[0], CLIENT_PATH);
    dirname(CLIENT_PATH);
    strcat(CLIENT_PATH, "/cliente");

    char SHM_NAME[80];
    strcpy(SHM_NAME, argv[1]);
    char SHM_DIR[80] = "/";
    strcat(SHM_DIR, SHM_NAME);

    int num_clients = atoi(argv[2]);

    /*
     * Shared memory
     *   NOTE: Opened in readonly mode because ger_cl will only read the
     * counters
     *         table from the shared memory and not modify anything
     */
    sem_t *shm_sem = sem_open(SHM_DIR, 0, 0600, 0);
    if (shm_sem == SEM_FAILED) {
        printf("Couldn't open semaphore '%s'\n", SHM_DIR);
    }

    int shm_fd = shm_open(SHM_DIR, O_RDONLY, 0600);
    if (shm_fd < 0) {
        printf("#ERROR# Couldn't open shared memory\n");
        exit(2);
    }

    memstruct_t *shm;
    shm = (memstruct_t *)mmap(0, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        printf("#ERROR# Couldn't map shared memory\n");
        exit(2);
    }
    /* ------end shared memory-------- */

    // Early out if there are no counters
    if (shm->numCounters < 1 || shm->activeCounters < 1) {
    NO_COUNTERS:
        printf("#ERROR# No counters available\n");
        goto EXIT;
    }

    memstruct_print(shm);

    int clientsCreated = 0;

    for (; num_clients > 0; num_clients--, ++clientsCreated) {
        if (shm->activeCounters < 1)
            goto NO_COUNTERS;

        int i, min, index;
        min = 9999999;
        index = -1;
        for (i = 0; i < shm->numCounters; i++) {
            if (shm->counters[i].currClients < min &&
                shm->counters[i].duration == -1 &&
                strcmp(shm->counters[i].fifo_name, "-")) {
                index = i;
                min = shm->counters[i].currClients;
            }
        }

        if (index < 0) {
            goto NO_COUNTERS;
        }

        char COUNTER_FIFO_NAME[80];
        strcpy(COUNTER_FIFO_NAME, shm->counters[index].fifo_name);

        char SEM_NAME[80] = "/";
        int helper = strcspn(COUNTER_FIFO_NAME, "_");
        strncat(SEM_NAME, COUNTER_FIFO_NAME + helper + 1,
                strlen(COUNTER_FIFO_NAME));

        sem_t *temp_sem = sem_open(SEM_NAME, 0, 0600, 0);
        if (temp_sem == SEM_FAILED) {
            printf("Couldn't open semaphore '%s'\n", SEM_NAME);
        }

        if (fork() == 0) {
            char counterNumber[5];
            sprintf(counterNumber, "%d", index + 1);

            execlp(CLIENT_PATH, CLIENT_PATH, COUNTER_FIFO_NAME, counterNumber,
                   SHM_NAME, NULL);
            printf("#ERROR# Couldn't exec '%s %s %s %s\n'", CLIENT_PATH,
                   COUNTER_FIFO_NAME, counterNumber, SHM_NAME);
            exit(1);
        }

        sem_wait(temp_sem);
        sem_post(temp_sem);

        printf("Clients left: %d\n", num_clients);
        printf("Clients created: %d\n", clientsCreated);
    }

    wait(NULL);

EXIT:
    if (munmap(shm, SHM_SIZE) < 0) {
        printf("#ERROR# Couldn't unmap shared memory\n");
    }

    exit(0);
}
