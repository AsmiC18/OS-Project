
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

static int failures = 0;

static void check(int condition, const char *message)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", message);

    if (!condition)
        failures++;
}

int main(void)
{
    ProcessMonitor monitor;
    MonitorSample sample;

    check(monitor_init(&monitor) == 0,
          "monitor initializes successfully");

    /* The current test process is guaranteed to exist. */
    check(monitor_sample(&monitor, getpid(), &sample) == 0,
          "monitor reads the current process");

    if (sample.valid) {
        check(sample.process.pid == getpid(),
              "sample contains the correct PID");
        check(sample.process.name[0] != '\0',
              "sample contains the process name");
        check(sample.memory_mb >= 0.0,
              "memory usage is non-negative");
        check(sample.cpu_percent >= 0.0,
              "CPU usage is non-negative");
    } else {
        check(0, "sample is marked valid");
    }

    check(monitor.history_count == 1,
          "first sample is stored in history");

    /* A second sample should update history and CPU baseline. */
struct timespec delay = {
    .tv_sec = 0,
    .tv_nsec = 100000000
};
nanosleep(&delay, NULL);
    check(monitor_sample(&monitor, getpid(), &sample) == 0,
          "monitor takes a second sample");

    check(monitor.history_count == 2,
          "second sample is stored in history");

    /* Invalid PID arguments must be rejected. */
    errno = 0;
    check(monitor_sample(&monitor, -1, &sample) == -1 && errno == EINVAL,
          "invalid PID is rejected");

    /* A PID that does not exist should fail gracefully. */
    errno = 0;
    check(monitor_sample(&monitor, 2147483647, &sample) == -1,
          "unavailable process is rejected");

    monitor_destroy(&monitor);

    check(monitor.entries == NULL,
          "monitor cleanup releases allocated entries");
    check(monitor.entry_count == 0,
          "monitor cleanup resets entry count");

    printf("\nMonitor tests: %d failure(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
