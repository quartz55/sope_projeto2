#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/wait.h>

#include "memstruct.h"
#include "vector.h"

#define SHM_SIZE 4096
char SHM_NAME[80];

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: %s <nome_mempartilhada> <num_clientes>\n", argv[0]);
        exit(3);
    }

    int num_clients = atoi(argv[2]);

    strcpy(SHM_NAME, argv[1]);
    char shm_dir[80] = "/";
    strcat(shm_dir, SHM_NAME);

    int shm_fd = shm_open(shm_dir, O_RDWR, 0600);
    if (shm_fd < 0) {
        printf("#ERROR# Couldn't open shared memory\n");
        exit(2);
    }

    memstruct_t *shm;
    shm = (memstruct_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        printf("#ERROR# Couldn't map shared memory\n");
        exit(2);
    }

    pthread_mutex_lock(&shm->mutx);

    if(shm->numCounters < 1) {
    NO_COUNTERS:
        printf("#ERROR# No counters available\n");
        pthread_mutex_unlock(&shm->mutx);
        goto EXIT;
    }

    memstruct_print(shm);

    pthread_mutex_unlock(&shm->mutx);


    for(; num_clients>0; num_clients--) {

        pthread_mutex_lock(&shm->mutx);

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

        printf("%d\n", min);

        if(index < 0){
            goto NO_COUNTERS;
        }

        char counter_fifo[80];
        strcpy(counter_fifo, shm->counters[index].fifo_name);

        ++shm->counters[index].currClients;

        pthread_mutex_unlock(&shm->mutx);

        if(fork() == 0) {
            execlp("./cliente", "./cliente", counter_fifo, SHM_NAME, NULL);
            printf("#ERROR# Couldn't exec './cliente %s %s\n'", counter_fifo, SHM_NAME);
            exit(1);
        }
    }

    /* wait(NULL); */

EXIT:
    if (munmap(shm, SHM_SIZE) < 0) {
        printf("#ERROR# Couldn't unmap shared memory\n");
    }

    exit(0);
}
