
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/resource.h>

#include "health.h"

#define MAX_PROCESSES 256
#define NAME_LENGTH 100

typedef struct {
    int pid;
    char name[NAME_LENGTH];
    char state;
    long rss_kb;
    long cpu_ticks;
    int priority;
    float cpu;
    float memory;
    int score;
} ProcessInfo;

static long read_total_cpu(void)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;

    char line[512];
    long total = 0;
    long value;

    if (fgets(line, sizeof(line), fp)) {
        char *p = line + 3;

        while (sscanf(p, "%ld", &value) == 1) {
            total += value;

            while (*p == ' ')
                p++;
            while (isdigit((unsigned char)*p))
                p++;
            while (*p == ' ')
                p++;
        }
    }

    fclose(fp);
    return total;
}

static int read_process(ProcessInfo *proc)
{
    char path[128];
    char line[2048];

    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");

    if (!fp)
        return -1;

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *right = strrchr(line, ')');
    if (!right || right[1] != ' ')
        return -1;

    long v[20] = {0};
    int n = sscanf(
        right + 2,
        "%c %ld %ld %ld %ld %ld %ld %ld %ld %ld "
        "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
        &proc->state,
        &v[0], &v[1], &v[2], &v[3], &v[4], &v[5],
        &v[6], &v[7], &v[8], &v[9], &v[10], &v[11],
        &v[12], &v[13], &v[14], &v[15], &v[16],
        &v[17], &v[18]);

    if (n < 13)
        return -1;

    /* /proc/PID/stat fields 14 and 15: utime and stime. */
    proc->cpu_ticks = v[10] + v[11];

    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    fp = fopen(path, "r");

    if (!fp)
        return -1;

    snprintf(proc->name, sizeof(proc->name), "unknown");
    proc->rss_kb = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Name:", 5) == 0)
            sscanf(line + 5, "%99s", proc->name);

        if (strncmp(line, "VmRSS:", 6) == 0)
            sscanf(line + 6, "%ld", &proc->rss_kb);
    }

    fclose(fp);

    errno = 0;
    proc->priority = getpriority(PRIO_PROCESS, proc->pid);

    if (errno != 0)
        return -1;

    return 0;
}

int calculate_health(float cpu, float memory, char state, int priority)
{
    int score = 100;

    if (cpu > 90.0f)
        score -= 30;
    else if (cpu > 70.0f)
        score -= 15;

    if (memory > 20.0f)
        score -= 30;
    else if (memory > 10.0f)
        score -= 15;

    if (state == 'Z')
        score -= 40;
    else if (state == 'T' || state == 't')
        score -= 20;

    if (priority > 10)
        score -= 10;

    if (score < 0)
        score = 0;

    return score;
}

static long read_rss_kb(int pid)
{
    char path[128], line[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    long rss = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }

    fclose(fp);
    return rss;
}

int measure_process_health(int pid, float *cpu_out, float *memory_out)
{
    if (pid <= 1 || !cpu_out || !memory_out)
        return -1;

    ProcessInfo proc = {0};
    proc.pid = pid;

    if (read_process(&proc) != 0)
        return -1;

    long ticks_before = proc.cpu_ticks;
    long total_before = read_total_cpu();

    sleep(1);

    ProcessInfo after = {0};
    after.pid = pid;

    if (read_process(&after) != 0)
        return -1;

    long total_after = read_total_cpu();

    if (total_before < 0 || total_after <= total_before)
        return -1;

    long tick_delta = after.cpu_ticks - ticks_before;
    if (tick_delta < 0)
        return -1;

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1)
        cpu_count = 1;

    *cpu_out = (float)tick_delta * 100.0f /
               (float)(total_after - total_before) *
               (float)cpu_count;

    long rss = read_rss_kb(pid);
    if (rss < 0)
        return -1;

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;

    long total_memory_kb = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &total_memory_kb);
            break;
        }
    }

    fclose(fp);

    if (total_memory_kb <= 0)
        return -1;

    *memory_out = (float)rss * 100.0f / (float)total_memory_kb;

    return calculate_health(
        *cpu_out, *memory_out, after.state, after.priority);
}

void show_health(void)
{
    ProcessInfo processes[MAX_PROCESSES];
    int count = 0;

    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("Cannot open /proc");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL &&
           count < MAX_PROCESSES) {
        const char *p = entry->d_name;

        if (!*p)
            continue;

        while (*p && isdigit((unsigned char)*p))
            p++;

        if (*p != '\0')
            continue;

        int pid = atoi(entry->d_name);
        if (pid <= 0)
            continue;

        ProcessInfo proc = {0};
        proc.pid = pid;

        if (read_process(&proc) == 0)
            processes[count++] = proc;
    }

    closedir(dir);

    long total_before = read_total_cpu();
    long before[MAX_PROCESSES];

    for (int i = 0; i < count; i++)
        before[i] = processes[i].cpu_ticks;

    sleep(1);

    long total_after = read_total_cpu();
    long total_delta = total_after - total_before;

    FILE *fp = fopen("/proc/meminfo", "r");
    long total_memory_kb = 0;
    char line[256];

    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%ld", &total_memory_kb);
                break;
            }
        }
        fclose(fp);
    }

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1)
        cpu_count = 1;

    printf("\n================================================================\n");
    printf("                    AEGISOS HEALTH MONITOR\n");
    printf("================================================================\n");
    printf("%-7s %-18s %-8s %-8s %-8s %-8s %-10s\n",
           "PID", "PROCESS", "CPU%", "MEM%", "PRIOR",
           "SCORE", "STATUS");
    printf("----------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        ProcessInfo after = {0};
        after.pid = processes[i].pid;

        if (read_process(&after) != 0)
            continue;

        long delta = after.cpu_ticks - before[i];

        if (total_delta > 0 && delta >= 0)
            after.cpu = (float)delta * 100.0f /
                        (float)total_delta * (float)cpu_count;

        if (total_memory_kb > 0)
            after.memory = (float)after.rss_kb * 100.0f /
                           (float)total_memory_kb;

        after.score = calculate_health(
            after.cpu, after.memory, after.state, after.priority);

        const char *status = after.score >= 70 ? "HEALTHY" :
                             after.score >= 40 ? "WARNING" : "CRITICAL";

        printf("%-7d %-18.18s %-8.2f %-8.2f %-8d %-8d %-10s\n",
               after.pid, after.name, after.cpu, after.memory,
               after.priority, after.score, status);
    }

    printf("================================================================\n");
}
