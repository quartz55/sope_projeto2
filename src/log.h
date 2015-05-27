#ifndef LOG_H
#define LOG_H

#include <time.h>
#include <stdio.h>
#include <string.h>

void clearLog(char *logname)
{
    /* Create file name */
    char filename[256];
    strcpy(filename, logname);
    strcat(filename, ".log");

    /* Open file in append mode */
    FILE *file;
    file = fopen(filename, "w");

    fprintf(file, "%-25s | %-7s | %-6s | %-25s | %-18s \n", "quando", "quem",
            "balcao", "o_que", "canal_criado/usado");
    int i;
    for (i = 0; i < 93; i++)
        fprintf(file, "-");
    fprintf(file, "\n");

    fclose(file);
}

void logLine(char *logname, int who, int counter, char *what, char *fifo_name)
{
    /* Create file name */
    char filename[256];
    strcpy(filename, logname);
    strcat(filename, ".log");

    /* Check if file exists */
    FILE *file;

    /* Open file in append mode */
    file = fopen(filename, "a");

    time_t now = time(NULL);

    char time[256];
    strcpy(time, ctime(&now));
    strtok(time, "\n");

    if (now != -1) {
        char quem[8];
        if (who)
            strcpy(quem, "Cliente");
        else
            strcpy(quem, "Balcao");

        fprintf(file, "%-25s | %-7s | %-6d | %-25s | %-18s \n", time, quem,
                counter, what, fifo_name);
    }

    fclose(file);
}

#endif /* LOG_H */
