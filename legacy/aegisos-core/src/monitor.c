#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/resource.h>

#include "monitor.h"


long get_total_cpu_time() {

    FILE *file = fopen("/proc/stat", "r");

    if (file == NULL) {
        return 0;
    }

    char line[512];

    fgets(line, sizeof(line), file);

    fclose(file);

    long user, nice, system, idle;
    long iowait, irq, softirq, steal;

    sscanf(line,
           "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
           &user,
           &nice,
           &system,
           &idle,
           &iowait,
           &irq,
           &softirq,
           &steal);

    return user + nice + system + idle +
           iowait + irq + softirq + steal;
}


long get_process_cpu_time(int pid) {

    char path[512];

    snprintf(path, sizeof(path),
             "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    char line[2048];

    fgets(line, sizeof(line), file);

    fclose(file);

    char *right_paren = strrchr(line, ')');

    if (right_paren == NULL) {
        return 0;
    }

    char *data = right_paren + 2;

    char state;
    long value;

    /*
       After the process name:
       field 3 = state
       fields 4-13 = other information
       field 14 = utime
       field 15 = stime
    */

    sscanf(data,
           "%c %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
           &state,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value,
           &value);

    /*
       Simpler approach:
       tokenize the remaining fields.
    */

    char *token;
    int field = 3;
    long utime = 0;
    long stime = 0;

    token = strtok(data, " ");

    while (token != NULL) {

        if (field == 14) {
            utime = atol(token);
        }

        if (field == 15) {
            stime = atol(token);
            break;
        }

        token = strtok(NULL, " ");
        field++;
    }

    return utime + stime;
}


void monitor_processes() {

    DIR *directory;
    struct dirent *entry;

    directory = opendir("/proc");

    if (directory == NULL) {
        perror("Unable to open /proc");
        return;
    }

    printf("\nCollecting CPU information...\n");

    /*
       First CPU measurement
    */

    long total_cpu_1 = get_total_cpu_time();

    /*
       Store process CPU times
    */

    int pids[500];
    long process_cpu_1[500];

    int count = 0;

    while ((entry = readdir(directory)) != NULL) {

        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        if (count >= 500) {
            break;
        }

        int pid = atoi(entry->d_name);

        pids[count] = pid;

        process_cpu_1[count] =
            get_process_cpu_time(pid);

        count++;
    }

    closedir(directory);

    /*
       Wait before second measurement
    */

    sleep(1);

    /*
       Second CPU measurement
    */

    long total_cpu_2 = get_total_cpu_time();

    printf("\n============================================================\n");
    printf("                AEGISOS RESOURCE MONITOR\n");
    printf("============================================================\n");

    printf("%-8s %-20s %-10s %-12s %-10s\n",
           "PID",
           "PROCESS",
           "CPU%",
           "MEMORY",
           "PRIORITY");

    printf("------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {

        int pid = pids[i];

        long process_cpu_2 =
            get_process_cpu_time(pid);

        long process_difference =
            process_cpu_2 - process_cpu_1[i];

        long total_difference =
            total_cpu_2 - total_cpu_1;

        double cpu_usage = 0;

        if (total_difference > 0) {

            cpu_usage =
                ((double) process_difference /
                 total_difference) * 100.0;
        }

        /*
           Get process information
        */

        char path[512];
        char line[256];

        char name[100] = "Unknown";
        long memory = 0;

        snprintf(path, sizeof(path),
                 "/proc/%d/status", pid);

        FILE *file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        while (fgets(line, sizeof(line), file)) {

            if (strncmp(line, "Name:", 5) == 0) {

                sscanf(line,
                       "Name:\t%99s",
                       name);
            }

            if (strncmp(line, "VmRSS:", 6) == 0) {

                sscanf(line,
                       "VmRSS:\t%ld",
                       &memory);
            }
        }

        fclose(file);

        int priority =
            getpriority(PRIO_PROCESS, pid);

        printf("%-8d %-20s %-10.2f %-12ld %-10d\n",
               pid,
               name,
               cpu_usage,
               memory,
               priority);
    }
}