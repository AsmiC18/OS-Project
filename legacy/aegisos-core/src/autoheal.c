
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>

#include "autoheal.h"
#include "logger.h"

void auto_heal_pid(int pid)
{
    if (pid <= 1)
    {
        printf("Invalid PID. Use a PID greater than 1.\n");
        return;
    }

    /* Only operate on a process that exists. */
    if (kill(pid, 0) != 0)
    {
        perror("Process does not exist or is inaccessible");
        return;
    }

    errno = 0;
    int old_priority = getpriority(PRIO_PROCESS, pid);

    if (errno != 0)
    {
        perror("Cannot read process priority");
        return;
    }

    printf("\n========================================\n");
    printf("       AEGISOS CONTROLLED RECOVERY\n");
    printf("========================================\n");
    printf("Target PID: %d\n", pid);
    printf("Original nice value: %d\n", old_priority);

    /*
     * Increasing the nice value to 5 lowers scheduling priority.
     * This is a controlled demonstration, not a general health fix.
     */
    if (setpriority(PRIO_PROCESS, pid, 5) != 0)
    {
        perror("Priority adjustment failed");

        log_recovery(
            pid, 0, 0.0f,
            "FAILED: priority adjustment"
        );

        printf("Recovery result: FAILED\n");
        return;
    }

    errno = 0;
    int new_priority = getpriority(PRIO_PROCESS, pid);

    if (errno == 0 && new_priority == 5)
    {
        printf("New nice value: %d\n", new_priority);
        printf("Recovery result: VERIFIED\n");

        log_recovery(
            pid, 100, 0.0f,
            "SUCCESS: priority adjustment verified"
        );
    }
    else
    {
        printf("Recovery result: VERIFICATION FAILED\n");

        log_recovery(
            pid, 0, 0.0f,
            "FAILED: priority verification"
        );
    }

    printf("========================================\n");
}
