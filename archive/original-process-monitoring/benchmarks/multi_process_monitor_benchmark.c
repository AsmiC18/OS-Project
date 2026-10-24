
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CHILDREN 64

static double elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

static int parse_long(const char *text, long min, long max, long *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < min || value > max) {
        return -1;
    }

    *out = value;
    return 0;
}

static void stop_children(pid_t *pids, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pids[i] > 0) {
            (void)kill(pids[i], SIGTERM);
        }
    }

    for (size_t i = 0; i < count; i++) {
        if (pids[i] > 0) {
            while (waitpid(pids[i], NULL, 0) < 0 && errno == EINTR) {
            }
        }
    }
}

int main(int argc, char **argv)
{
    long process_count = 4;
    long rounds = 100;

    if (argc > 3 ||
        (argc >= 2 && parse_long(argv[1], 1, MAX_CHILDREN,
                                 &process_count) != 0) ||
        (argc == 3 && parse_long(argv[2], 10, 10000, &rounds) != 0)) {
        fprintf(stderr,
                "Usage: %s [process-count:1-64] [rounds:10-10000]\n",
                argv[0]);
        return 2;
    }

    pid_t pids[MAX_CHILDREN] = {0};
    size_t created = 0;

    for (long i = 0; i < process_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            stop_children(pids, created);
            return 1;
        }

        if (pid == 0) {
            for (;;) {
                pause();
            }
        }

        pids[created++] = pid;
    }

    ProcessMonitor monitor;
    if (monitor_init(&monitor) != 0) {
        perror("monitor_init");
        stop_children(pids, created);
        return 1;
    }

    MonitorSample sample;
    struct timespec wall_start, wall_end;
    struct timespec cpu_start, cpu_end;

    if (clock_gettime(CLOCK_MONOTONIC, &wall_start) != 0 ||
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) != 0) {
        perror("clock_gettime");
        monitor_destroy(&monitor);
        stop_children(pids, created);
        return 1;
    }

    for (long round = 0; round < rounds; round++) {
        for (size_t i = 0; i < created; i++) {
            if (monitor_sample(&monitor, pids[i], &sample) != 0) {
                perror("monitor_sample");
                monitor_destroy(&monitor);
                stop_children(pids, created);
                return 1;
            }
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0 ||
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end) != 0) {
        perror("clock_gettime");
        monitor_destroy(&monitor);
        stop_children(pids, created);
        return 1;
    }

    double wall_seconds = elapsed_seconds(wall_start, wall_end);
    double cpu_seconds = elapsed_seconds(cpu_start, cpu_end);
    double total_samples = (double)created * (double)rounds;
    double throughput = wall_seconds > 0.0
                            ? total_samples / wall_seconds
                            : 0.0;

    printf("processes,rounds,total_samples,wall_seconds,cpu_seconds,"
           "samples_per_second\n");
    printf("%zu,%ld,%.0f,%.6f,%.6f,%.2f\n",
           created, rounds, total_samples, wall_seconds,
           cpu_seconds, throughput);

    monitor_destroy(&monitor);
    stop_children(pids, created);
    return 0;
}

