#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/resource.h>

long get_total_cpu()
{
    FILE *file = fopen("/proc/stat", "r");

    if (file == NULL)
        return 0;

    char line[512];
    long user, nice, system, idle, iowait, irq, softirq, steal;

    fgets(line, sizeof(line), file);
    fclose(file);

    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle,
           &iowait, &irq, &softirq, &steal);

    return user + nice + system + idle +
           iowait + irq + softirq + steal;
}

long get_process_cpu(int pid)
{
    char path[512];

    sprintf(path, "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;

    char line[2048];

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    fclose(file);

    char *closing_bracket = strrchr(line, ')');

    if (closing_bracket == NULL)
        return 0;

    char *data = closing_bracket + 2;

    char state;
    long values[20];

    int count = sscanf(data,
        "%c %ld %ld %ld %ld %ld %ld %ld %ld %ld "
        "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
        &state,
        &values[0], &values[1], &values[2], &values[3],
        &values[4], &values[5], &values[6], &values[7],
        &values[8], &values[9], &values[10], &values[11],
        &values[12], &values[13], &values[14], &values[15],
        &values[16], &values[17], &values[18]);

    if (count < 14)
        return 0;

    /*
     * utime = field 14
     * stime = field 15
     */
    return values[10] + values[11];
}

int calculate_health(float cpu, float memory, char state, int priority)
{
    int score = 100;

    /* CPU penalty */
    if (cpu > 80)
        score -= 30;
    else if (cpu > 50)
        score -= 15;

    /* Memory penalty */
    if (memory > 80)
        score -= 30;
    else if (memory > 50)
        score -= 15;

    /* Process state */
    if (state == 'Z')
        score -= 40;
    else if (state == 'T')
        score -= 20;

    /* Very low priority */
    if (priority > 10)
        score -= 10;

    if (score < 0)
        score = 0;

    return score;
}

void show_health()
{
    DIR *dir = opendir("/proc");

    if (dir == NULL)
    {
        perror("Cannot open /proc");
        return;
    }

    printf("\n================================================================\n");
    printf("                    AEGISOS HEALTH MONITOR\n");
    printf("================================================================\n");

    printf("%-7s %-18s %-8s %-8s %-8s %-8s %-10s\n",
           "PID", "PROCESS", "CPU%", "MEM%", "PRIOR", "SCORE", "STATUS");

    printf("----------------------------------------------------------------\n");

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < 15)
    {
        if (entry->d_type != DT_DIR)
            continue;

        int pid = atoi(entry->d_name);

        if (pid <= 0)
            continue;

        char path[512];
        char name[100] = "unknown";
        char state = 'R';

        sprintf(path, "/proc/%d/status", pid);

        FILE *file = fopen(path, "r");

        if (file == NULL)
            continue;

        char line[256];
        long memory = 0;

        while (fgets(line, sizeof(line), file))
        {
            if (strncmp(line, "Name:", 5) == 0)
                sscanf(line, "Name:\t%99s", name);

            if (strncmp(line, "State:", 6) == 0)
                sscanf(line, "State:\t%c", &state);

            if (strncmp(line, "VmRSS:", 6) == 0)
                sscanf(line, "VmRSS:\t%ld", &memory);
        }

        fclose(file);

        /*
         * Convert process memory into a simple percentage
         * relative to total system memory.
         */
        FILE *meminfo = fopen("/proc/meminfo", "r");

        long total_memory = 1;

        if (meminfo != NULL)
        {
            while (fgets(line, sizeof(line), meminfo))
            {
                if (strncmp(line, "MemTotal:", 9) == 0)
                {
                    sscanf(line, "MemTotal: %ld", &total_memory);
                    break;
                }
            }

            fclose(meminfo);
        }

        float memory_percent =
            ((float)memory / (float)total_memory) * 100.0;

        /*
         * Measure CPU twice with a 1-second interval.
         */
        long total1 = get_total_cpu();
        long process1 = get_process_cpu(pid);

        sleep(1);

        long total2 = get_total_cpu();
        long process2 = get_process_cpu(pid);

        long total_diff = total2 - total1;
        long process_diff = process2 - process1;

        float cpu = 0;

        if (total_diff > 0)
        {
            cpu = ((float)process_diff /
                   (float)total_diff) * 100.0;
        }

        int priority = getpriority(PRIO_PROCESS, pid);

        int score =
            calculate_health(cpu,
                             memory_percent,
                             state,
                             priority);

        char *status;

        if (score >= 70)
            status = "HEALTHY";
        else if (score >= 40)
            status = "WARNING";
        else
            status = "CRITICAL";

        printf("%-7d %-18s %-8.2f %-8.2f %-8d %-8d %-10s\n",
               pid,
               name,
               cpu,
               memory_percent,
               priority,
               score,
               status);

        count++;
    }

    printf("================================================================\n");

    closedir(dir);
}