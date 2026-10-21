
#define _POSIX_C_SOURCE 200809L

#include "supervisor.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int supervisor_run(const SupervisorConfig *config)
{
    if (!config || !config->argv || !config->argv[0] ||
        config->argv[0][0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    unsigned restarts = 0;

    for (;;) {
        pid_t child = fork();

        if (child < 0) {
            perror("fork");
            return -1;
        }

        if (child == 0) {
            execvp(config->argv[0], config->argv);
            perror("execvp");
            _exit(127);
        }

        printf("[supervisor] started PID %ld: %s\n",
               (long)child, config->argv[0]);
        fflush(stdout);

        int status;
        pid_t waited;

        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);

        if (waited < 0) {
            perror("waitpid");
            return -1;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            puts("[supervisor] process exited successfully");
            return 0;
        }

        if (WIFEXITED(status)) {
            printf("[supervisor] exited with status %d\n",
                   WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[supervisor] terminated by signal %d\n",
                   WTERMSIG(status));
        }

        if (restarts >= config->max_restarts) {
            printf("[supervisor] restart limit reached (%u)\n",
                   config->max_restarts);
            return 1;
        }

        restarts++;

        unsigned delay = config->backoff_seconds;
        for (unsigned i = 1; i < restarts && delay < 60; i++) {
            if (delay > 30) {
                delay = 60;
                break;
            }
            delay *= 2;
        }
        if (delay > 60)
            delay = 60;

        printf("[supervisor] restart %u/%u in %u second(s)\n",
               restarts, config->max_restarts, delay);

        struct timespec remaining = {
            .tv_sec = delay,
            .tv_nsec = 0
        };

        while (delay > 0 && nanosleep(&remaining, &remaining) < 0) {
            if (errno != EINTR) {
                perror("nanosleep");
                return -1;
            }
        }
    }
}
