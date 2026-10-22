
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>

#include "healing.h"
#include "process.h"
#include "logger.h"

void analyze_process_health(int pid, int score, float cpu)
{
    printf("\n--------------------------------------------\n");
    printf("       AEGISOS SELF-HEALING ENGINE\n");
    printf("--------------------------------------------\n");

    if (pid <= 1 || score < 0 || score > 100 || cpu < 0.0f)
    {
        printf("Invalid PID, health score, or CPU value.\n");
        return;
    }

    if (kill(pid, 0) != 0)
    {
        perror("Target process unavailable");
        return;
    }

    printf("PID           : %d\n", pid);
    printf("CPU Usage     : %.2f%%\n", cpu);
    printf("Health Score  : %d/100\n", score);

    if (score >= 70)
    {
        printf("Status        : HEALTHY\n");
        printf("Action        : No healing required.\n");
        return;
    }

    if (score >= 40)
    {
        printf("Status        : WARNING\n");

       errno = 0;
        int old_priority = getpriority(PRIO_PROCESS, pid);

        if (errno != 0)
        {
            perror("Cannot read process priority");
            return;
        }

printf("Original nice value: %d\n", old_priority);

        printf("Action        : Adjusting priority to nice value 5.\n");

        if (setpriority(PRIO_PROCESS, pid, 5) != 0)
        {
            perror("Priority adjustment failed");

            log_recovery(pid, score, cpu,
                         "FAILED: priority adjustment");
            return;
        }

        errno = 0;
        int new_priority = getpriority(PRIO_PROCESS, pid);

        if (errno == 0 && new_priority == 5)
        {
            printf("Healing       : Priority adjustment verified.\n");

            log_recovery(pid, score, cpu,
                         "SUCCESS: priority adjustment verified");
        }
        else
        {
            printf("Healing       : Priority verification failed.\n");

            log_recovery(pid, score, cpu,
                         "FAILED: priority verification");
        }

        return;
    }

    printf("Status        : CRITICAL\n");
    printf("Action        : Requesting process suspension.\n");

    /*
     * Verify the process state after sending SIGSTOP.
     * This action can interrupt the target, so only use it on
     * a process you intentionally selected for testing.
     */
    if (kill(pid, SIGSTOP) != 0)
    {
        perror("Process suspension failed");

        log_recovery(pid, score, cpu,
                     "FAILED: process suspension");
        return;
    }

    printf("Healing       : Suspension signal sent.\n");

    log_recovery(pid, score, cpu,
                 "SUCCESS: suspension signal sent");
}
