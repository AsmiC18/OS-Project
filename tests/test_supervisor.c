
#define _POSIX_C_SOURCE 200809L

#include "supervisor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *description)
{
    if (condition) {
        printf("PASS: %s\n", description);
    } else {
        printf("FAIL: %s\n", description);
        failures++;
    }
}

static int run_supervisor(char *const argv[],
                          unsigned max_restarts,
                          unsigned backoff_seconds)
{
    SupervisorConfig config = {
        .argv = argv,
        .max_restarts = max_restarts,
        .backoff_seconds = backoff_seconds
    };

    return supervisor_run(&config);
}

int main(void)
{
    char *success_argv[] = {"/bin/sh", "-c", "exit 0", NULL};
    char *failure_argv[] = {"/bin/sh", "-c", "exit 7", NULL};
    char *missing_argv[] = {"/definitely/not/a/real/program", NULL};

    check(run_supervisor(success_argv, 0, 0) == 0,
          "successful child exits without restarting");

    check(run_supervisor(failure_argv, 0, 0) == 1,
          "failed child stops when restart limit is zero");

    check(run_supervisor(missing_argv, 0, 0) == 1,
          "missing executable is handled as child failure");

    errno = 0;
    check(supervisor_run(NULL) == -1 && errno == EINVAL,
          "null configuration is rejected");

    SupervisorConfig no_argv = {
        .argv = NULL,
        .max_restarts = 0,
        .backoff_seconds = 0
    };
    errno = 0;
    check(supervisor_run(&no_argv) == -1 && errno == EINVAL,
          "missing argument vector is rejected");

    char *empty_argv[] = {"", NULL};
    SupervisorConfig empty_program = {
        .argv = empty_argv,
        .max_restarts = 0,
        .backoff_seconds = 0
    };
    errno = 0;
    check(supervisor_run(&empty_program) == -1 && errno == EINVAL,
          "empty program name is rejected");

    printf("\nSupervisor tests: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
