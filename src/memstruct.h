#ifndef MEMSTRUCT_H
#define MEMSTRUCT_H

#include <stdio.h>
#include <pthread.h>
#include "vector.h"

#define SHM_SIZE (1<<20)

typedef struct counter_t
{
    int i;
    int startTime;
    int duration;
    char fifo_name[80];
    int currClients;
    int servedClients;
    int medTime;
}counter_t;

typedef struct memstruct_t
{
    int startTime;
    int activeCounters;
    int numCounters;
    counter_t counters[200];
}memstruct_t;

inline void counter_print(counter_t *c)
{
    printf(" %-5d | %-12d | %-7d | %-14s | %-15d | %-15d | %-12d\n",
           c->i+1,
           c->startTime,
           c->duration,
           c->fifo_name,
           c->currClients,
           c->servedClients,
           c->medTime);
}

inline void memstruct_print(memstruct_t *m)
{
    int i;
    for(i=0; i < 99; i++)
        printf("-");
    printf("\n");

    printf("Start time: %d\n", m->startTime);
    printf("Counters: %d (%d open)\n\n", m->numCounters, m->activeCounters);

    printf("%-6s | %-22s | %-14s | %-33s | %-12s\n",
           "Balcao",
           "        Abertura",
           "     Nome",
           "            Num clientes",
           "Tempo medio");
    printf("%-6s | %-12s | %-7s | %-14s | %-15s | %-15s | %-12s\n",
           " #",
           "    Tempo",
           "Duracao",
           "     FIFO",
           "em atendimento",
           "  ja atendidos",
           "atendimento");

    for(i=0; i < 99; i++)
        printf("-");
    printf("\n");

    for(i=0; i < m->numCounters; i++)
        counter_print(&m->counters[i]);

    for(i=0; i < 99; i++)
        printf("-");
    printf("\n");
}

#endif /* MEMSTRUCT_H */
