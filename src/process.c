#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

void list_processes() {

    DIR *directory;
    struct dirent *entry;

    directory = opendir("/proc");

    if (directory == NULL) {
        perror("Unable to open /proc");
        return;
    }

    printf("\n========================================\n");
    printf("        AEGISOS PROCESS MANAGER\n");
    printf("========================================\n");

    printf("%-10s %-25s %-10s\n", "PID", "PROCESS", "STATE");
    printf("----------------------------------------\n");

    while ((entry = readdir(directory)) != NULL) {

        /* Check whether directory name is a number */
        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        char path[512];

        snprintf(path, sizeof(path),
                 "/proc/%s/status", entry->d_name);

        FILE *file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        char line[256];
        char name[100] = "Unknown";
        char state[50] = "Unknown";

        while (fgets(line, sizeof(line), file)) {

            if (strncmp(line, "Name:", 5) == 0) {

                sscanf(line, "Name:\t%s", name);
            }

            if (strncmp(line, "State:", 6) == 0) {

                sscanf(line, "State:\t%49[^\n]", state);
            }
        }

        fclose(file);

        printf("%-10s %-25s %-10s\n",
               entry->d_name,
               name,
               state);
    }

    closedir(directory);
}