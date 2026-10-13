#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include "autoheal.h"
#include "process.h"
#include "logger.h"
#include <sys/resource.h>
#define _GNU_SOURCE

void auto_heal()
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir("/proc");

    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    printf("\n========================================\n");
    printf("        AEGISOS AUTO-HEAL ENGINE\n");
    printf("========================================\n");

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type != DT_DIR)
            continue;

        int pid = atoi(entry->d_name);

        if (pid <= 0)
            continue;

        char path[256];
        char state;
        int priority;

        /* Read process state */
        snprintf(path, sizeof(path),
                 "/proc/%d/stat", pid);

        FILE *fp = fopen(path, "r");

        if (fp == NULL)
            continue;

        char name[100];

        if (fscanf(fp, "%d %99s %c",
                   &pid, name, &state) != 3)
        {
            fclose(fp);
            continue;
        }

        fclose(fp);

        /* Read priority */
        priority = getpriority(PRIO_PROCESS, pid);

        /*
         * Simple health rule:
         * stopped process = unhealthy
         * very low priority = warning
         */

        int score = 100;

        if (state == 'T')
            score -= 40;

        if (priority > 10)
            score -= 10;

        printf("\nPID: %d\n", pid);
        printf("Process: %s\n", name);
        printf("State: %c\n", state);
        printf("Priority: %d\n", priority);
        printf("Health Score: %d\n", score);

        /*
         * Healing rule
         */

        if (score < 60)
        {
            printf("Action: Adjusting priority...\n");

            set_process_priority(pid, 5);

            log_recovery(
                pid,
                score,
                0.0,
                "Automatic priority adjustment"
            );

            printf("Recovery: Applied\n");
        }
        else
        {
            printf("Action: No recovery required\n");
        }
    }

    closedir(dir);

    printf("\n========================================\n");
    printf("        AUTO-HEAL SCAN COMPLETE\n");
    printf("========================================\n");
}