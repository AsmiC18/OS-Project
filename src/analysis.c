#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

#include "analysis.h"
#include "process.h"

void analyze_process(int pid)
{
    int before_priority;
    int after_priority;

    printf("\n============================================\n");
    printf("       AEGISOS PROCESS ANALYSIS\n");
    printf("============================================\n");

    /* Capture BEFORE state */
    before_priority = getpriority(PRIO_PROCESS, pid);

    if (before_priority == -1)
    {
        printf("Unable to access PID %d\n", pid);
        return;
    }

    printf("\nBEFORE HEALING\n");
    printf("--------------------------------------------\n");
    printf("PID       : %d\n", pid);
    printf("Priority  : %d\n", before_priority);

    /*
     * For this first automatic version,
     * we use priority as the measurable
     * recovery parameter.
     */

    printf("\nApplying recovery action...\n");

    set_process_priority(pid, before_priority + 5);

    sleep(1);

    /* Capture AFTER state */
    after_priority = getpriority(PRIO_PROCESS, pid);

    printf("\nAFTER HEALING\n");
    printf("--------------------------------------------\n");
    printf("PID       : %d\n", pid);
    printf("Priority  : %d\n", after_priority);

    printf("\n============================================\n");
    printf("          RECOVERY RESULT\n");
    printf("============================================\n");

    if (after_priority != before_priority)
    {
        printf("Recovery Status : SUCCESS\n");
        printf("Priority Change : %d -> %d\n",
               before_priority,
               after_priority);
    }
    else
    {
        printf("Recovery Status : NO CHANGE\n");
    }

    printf("============================================\n");
}