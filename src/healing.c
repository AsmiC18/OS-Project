#include <stdio.h>
#include "healing.h"
#include "process.h"
#include "logger.h"

void analyze_process_health(int pid, int score, float cpu)
{
    printf("\n--------------------------------------------\n");
    printf("       AEGISOS SELF-HEALING ENGINE\n");
    printf("--------------------------------------------\n");

    printf("PID           : %d\n", pid);
    printf("CPU Usage     : %.2f%%\n", cpu);
    printf("Health Score  : %d/100\n", score);

    if (score >= 70)
    {
        printf("Status        : HEALTHY\n");
        printf("Action        : No healing required.\n");
    }
    else if (score >= 40)
{
    printf("Status        : WARNING\n");
    printf("Action        : Reducing process priority...\n");

    set_process_priority(pid, 5);

    log_recovery(pid, score, cpu, "Priority adjusted");

    printf("Healing       : Priority adjusted.\n");
}
    else
{
    printf("Status        : CRITICAL\n");
    printf("Action        : Suspending process...\n");

    suspend_process(pid);

    log_recovery(pid, score, cpu, "Process suspended");

    printf("Healing       : Process suspended.\n");
    }
}