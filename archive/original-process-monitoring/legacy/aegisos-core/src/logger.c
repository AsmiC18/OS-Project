#include <stdio.h>
#include <time.h>
#include "logger.h"

void log_recovery(int pid, int score, float cpu, const char *action)
{
    FILE *file = fopen("recovery.log", "a");

    if (file == NULL)
    {
        printf("Unable to open recovery log.\n");
        return;
    }

    time_t current_time = time(NULL);

    fprintf(file,
            "PID: %d | Score: %d | CPU: %.2f%% | Action: %s | Time: %s",
            pid,
            score,
            cpu,
            action,
            ctime(&current_time));

    fclose(file);
}

void show_logs()
{
    FILE *file = fopen("recovery.log", "r");

    if (file == NULL)
    {
        printf("\nNo recovery logs found.\n");
        return;
    }

    char line[512];

    printf("\n============================================\n");
    printf("          AEGISOS RECOVERY LOG\n");
    printf("============================================\n");

    while (fgets(line, sizeof(line), file))
    {
        printf("%s", line);
    }

    printf("============================================\n");

    fclose(file);
}