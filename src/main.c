
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include "anomaly.h"
#include "policy.h"
#include "supervisor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

static void print_usage(const char *program)
{
    printf("AegisOS: Linux process monitoring and supervision prototype\n");
    printf("Usage:\n");
    printf("  %s --help\n", program);
    printf("  %s --version\n", program);
    printf("  %s --monitor PID [COUNT]\n", program);
    printf("  %s --supervise PROGRAM [ARGS...]\n", program);
}

int main(int argc, char **argv)
{
    /* Version */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("AegisOS 0.1.0");
        return 0;
    }

    /* Help */
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    /* Supervise a child program and restart it after failure */
    if (argc >= 3 && strcmp(argv[1], "--supervise") == 0) {
        SupervisorConfig config = {
            .argv = &argv[2],
            .max_restarts = 3,
            .backoff_seconds = 1
        };

        return supervisor_run(&config);
    }

    /* Monitor an existing process */
    if (argc >= 3 && strcmp(argv[1], "--monitor") == 0) {
        char *end = NULL;

        errno = 0;
        long pid_value = strtol(argv[2], &end, 10);

        if (errno != 0 || end == argv[2] || *end != '\0' ||
            pid_value <= 0 || pid_value > INT_MAX) {
            fprintf(stderr, "Error: invalid PID '%s'\n", argv[2]);
            return 2;
        }

        unsigned long count = 10;

        if (argc >= 4) {
            errno = 0;
            end = NULL;
            count = strtoul(argv[3], &end, 10);

            if (errno != 0 || end == argv[3] || *end != '\0' ||
                count == 0 || count > 100000) {
                fprintf(stderr, "Error: invalid sample count '%s'\n",
                        argv[3]);
                return 2;
            }
        }

        if (argc > 4) {
            fprintf(stderr, "Error: too many arguments for --monitor\n");
            print_usage(argv[0]);
            return 2;
        }

        ProcessMonitor monitor;

        if (monitor_init(&monitor) != 0) {
            perror("monitor_init");
            return 1;
        }

        AnomalyDetector detector;
        anomaly_detector_init(&detector, 80.0, 512.0, 3);

        printf("AegisOS monitoring PID %ld; sampling every second.\n",
               pid_value);
        puts("CPU% is normalized per logical CPU; first sample establishes baseline.");
        puts("Press Ctrl+C to stop.");

        for (unsigned long i = 0; i < count; i++) {
            MonitorSample sample;

            if (monitor_sample(&monitor, (pid_t)pid_value, &sample) != 0) {
                perror("monitor_sample");
                monitor_destroy(&monitor);
                return 1;
            }

            printf("PID=%ld name=%s state=%c CPU=%.2f%% RSS=%.2f MB\n",
                   (long)sample.process.pid,
                   sample.process.name,
                   sample.process.state,
                   sample.cpu_percent,
                   sample.memory_mb);

            Anomaly anomaly;
            int found = anomaly_detector_evaluate(
                &detector, &sample, &anomaly);

            if (found > 0) {
                printf("ANOMALY: %s\n", anomaly.explanation);
            } else if (found < 0) {
                fprintf(stderr, "Warning: anomaly evaluation failed\n");
            }

            if (i + 1 < count) {
                sleep(1);
            }
        }

        monitor_destroy(&monitor);
        return 0;
    }

    fprintf(stderr, "Error: invalid command or missing arguments\n");
    print_usage(argv[0]);
    return 2;
}
