
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
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

static int sleep_seconds(long seconds)
{
    struct timespec delay = {
        .tv_sec = seconds,
        .tv_nsec = 0
    };

    while (nanosleep(&delay, &delay) != 0) {
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
                "Usage: %s <delay-seconds:0-3600> <exit-code:0-255|signal:1-31>\n",
                argv[0]);
        return 2;
    }

    long delay_seconds;
    long failure_value;

    if (parse_long(argv[1], 0, 3600, &delay_seconds) != 0 ||
        parse_long(argv[2], 0, 255, &failure_value) != 0) {
        fprintf(stderr,
                "Invalid arguments. Delay must be 0-3600 seconds; "
                "exit code must be 0-255.\n");
        return 2;
    }

    printf("crash_worker: waiting %ld seconds before exit with code %ld\n",
           delay_seconds, failure_value);
    fflush(stdout);

    if (sleep_seconds(delay_seconds) != 0) {
        perror("nanosleep");
        return 1;
    }

    printf("crash_worker: exiting now\n");
    fflush(stdout);

    return (int)failure_value;
}
