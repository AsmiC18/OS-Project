
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static double seconds_between(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

static int sleep_for(double seconds)
{
    if (seconds <= 0.0) {
        return 0;
    }

    struct timespec request = {
        .tv_sec = (time_t)seconds,
        .tv_nsec = (long)((seconds - (double)(time_t)seconds) * 1e9)
    };

    while (nanosleep(&request, &request) != 0) {
        if (errno != EINTR) {
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <cpu-duty-percent:1-100> <duration-seconds:1-3600>\n",
                argv[0]);
        return 2;
    }

    long duty_percent;
    long duration_seconds;

    if (parse_long(argv[1], 1, 100, &duty_percent) != 0 ||
        parse_long(argv[2], 1, 3600, &duration_seconds) != 0) {
        fprintf(stderr, "Invalid arguments. Duty must be 1-100 and "
                        "duration must be 1-3600 seconds.\n");
        return 2;
    }

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        return 1;
    }

    printf("cpu_worker: duty=%ld%% duration=%lds\n",
           duty_percent, duration_seconds);
    fflush(stdout);

    const double total_duration = (double)duration_seconds;
    const double busy_duration = duty_percent / 100.0;
    const double idle_duration = 1.0 - busy_duration;
    volatile unsigned long work = 0;

    while (1) {
        struct timespec cycle_start;
        if (clock_gettime(CLOCK_MONOTONIC, &cycle_start) != 0) {
            perror("clock_gettime");
            return 1;
        }

        double elapsed = seconds_between(start, cycle_start);
        if (elapsed >= total_duration) {
            break;
        }

        double remaining = total_duration - elapsed;
        double busy_time = busy_duration < remaining
                               ? busy_duration : remaining;

        struct timespec busy_start = cycle_start;
        struct timespec busy_now;

        do {
            for (unsigned long i = 0; i < 10000UL; i++) {
                work = work * 1664525UL + 1013904223UL;
            }

            if (clock_gettime(CLOCK_MONOTONIC, &busy_now) != 0) {
                perror("clock_gettime");
                return 1;
            }
        } while (seconds_between(busy_start, busy_now) < busy_time);

        if (clock_gettime(CLOCK_MONOTONIC, &busy_now) != 0) {
            perror("clock_gettime");
            return 1;
        }

        elapsed = seconds_between(start, busy_now);
        remaining = total_duration - elapsed;

        if (remaining <= 0.0) {
            break;
        }

        double idle_time = idle_duration < remaining
                               ? idle_duration : remaining;

        if (sleep_for(idle_time) != 0) {
            perror("nanosleep");
            return 1;
        }
    }

    printf("cpu_worker: completed (checksum=%lu)\n", work);
    return 0;
}
