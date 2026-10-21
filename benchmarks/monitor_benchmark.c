
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static double elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

static int parse_sample_count(const char *text, long *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < 100 || value > 1000000) {
        return -1;
    }

    *out = value;
    return 0;
}

int main(int argc, char **argv)
{
    long sample_count = 1000;

    if (argc > 2 ||
        (argc == 2 && parse_sample_count(argv[1], &sample_count) != 0)) {
        fprintf(stderr, "Usage: %s [sample-count:100-1000000]\n", argv[0]);
        return 2;
    }

    ProcessMonitor monitor;
    if (monitor_init(&monitor) != 0) {
        perror("monitor_init");
        return 1;
    }

    MonitorSample sample;
    struct timespec wall_start, wall_end;
    struct timespec cpu_start, cpu_end;

    if (clock_gettime(CLOCK_MONOTONIC, &wall_start) != 0 ||
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) != 0) {
        perror("clock_gettime");
        monitor_destroy(&monitor);
        return 1;
    }

    for (long i = 0; i < sample_count; i++) {
        if (monitor_sample(&monitor, getpid(), &sample) != 0) {
            perror("monitor_sample");
            monitor_destroy(&monitor);
            return 1;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0 ||
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end) != 0) {
        perror("clock_gettime");
        monitor_destroy(&monitor);
        return 1;
    }

    double wall_seconds = elapsed_seconds(wall_start, wall_end);
    double cpu_seconds = elapsed_seconds(cpu_start, cpu_end);
    double samples_per_second =
        wall_seconds > 0.0 ? (double)sample_count / wall_seconds : 0.0;

    printf("%ld,%.6f,%.6f,%.2f,%.3f\n",
           sample_count,
           wall_seconds,
           cpu_seconds,
           samples_per_second,
           sample.memory_mb);

    monitor_destroy(&monitor);
    return 0;
}
