#ifndef LOG_H
#define LOG_H

#include <time.h>
#include <stdio.h>
#include <string.h>

void myLog(char *logname, int who, int counter, char *what, char *fifo_name)
{
    /* Create file name */
    char filename[256];
    strcpy(filename, logname);
    strcat(filename, ".log");

    /* Check if file exists */
    int exists = 0;
    FILE *file;
    file = fopen(filename, "r");
    if(file != NULL)
    {
        exists = 1;
        fclose(file);
    }

    /* Open file in append mode */
    file = fopen(filename, "a");
    if(!exists)
    {
        fprintf(file, "%-25s | %-7s | %-6s | %-25s | %-18s \n", "quando", "quem", "balcao", "o_que", "canal_criado/usado");
        int i;
        for(i=0; i < 81; i++)
            fprintf(file, "-");
        fprintf(file, "\n");
    }

    time_t now = time(NULL);

    char time[256];
    strcpy(time, ctime(&now));
    strtok(time, "\n");

    if(now != -1)
    {
        char quem[8];
        if(who) strcpy(quem, "Cliente");
        else strcpy(quem, "Balcao");

        fprintf(file, "%-25s | %-7s | %-6d | %-25s | %-18s \n", time, quem, counter, what, fifo_name);
    }
}

#endif /* LOG_H */
