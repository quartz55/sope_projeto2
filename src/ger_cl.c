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

#include "memstruct.h"
#include "vector.h"

char SHM_NAME[80];

int main(int argc, char *argv[])
{
    if(argc != 3) {
        printf("Usage: %s <nome_mempartilhada> <num_clientes>\n", argv[0]);
        exit(3);
    }

    int num_clients = atoi(argv[2]);

    // Get path to 'client'
    char client_path[256];
    realpath(argv[0], client_path);
    dirname(client_path);
    strcat(client_path, "/cliente");
    /* printf("%s\n", client_path); */
    /* -- */

    strcpy(SHM_NAME, argv[1]);
    char shm_dir[80] = "/";
    strcat(shm_dir, SHM_NAME);

    /*
     * Shared memory
     *   NOTE: Opened in readonly because ger_cl will only read the counters
     *         table from the shared memory and not modify anything
     */
    int shm_fd = shm_open(shm_dir, O_RDONLY, 0600);
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
    if(shm->numCounters < 1) {
    NO_COUNTERS:
        printf("#ERROR# No counters available\n");
        goto EXIT;
    }

    memstruct_print(shm);

    int clientsCreated = 0;

    for(; num_clients>0; num_clients--, ++clientsCreated)
    {
        int i, min, index;
        min = 9999999;
        index = -1;
        for(i=0; i < shm->numCounters; i++) {
            if(shm->counters[i].currClients < min &&
               shm->counters[i].duration == -1 &&
               strcmp(shm->counters[i].fifo_name, "-")) {
                index = i;
                min = shm->counters[i].currClients;
            }
        }

        if(index < 0){
            goto NO_COUNTERS;
        }

        char counter_fifo[80];
        strcpy(counter_fifo, shm->counters[index].fifo_name);

        if(fork() == 0) {
            char counterNumber[5];
            sprintf(counterNumber, "%d", index+1);
            execlp(client_path,
                   client_path,
                   counter_fifo,
                   counterNumber,
                   SHM_NAME,
                   NULL);
            printf("#ERROR# Couldn't exec '%s %s %s %s\n'",
                   client_path,
                   counter_fifo,
                   counterNumber,
                   SHM_NAME);
            exit(1);
        }

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
