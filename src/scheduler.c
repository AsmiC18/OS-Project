#include <stdio.h>
#include "scheduler.h"

void fcfs_scheduling()
{
    int n;
    int burst[20];
    int waiting[20];
    int turnaround[20];

    printf("\n============================================\n");
    printf("          FCFS CPU SCHEDULING\n");
    printf("============================================\n");

    printf("Enter number of processes: ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++)
    {
        printf("Enter burst time for P%d: ", i + 1);
        scanf("%d", &burst[i]);
    }

    waiting[0] = 0;

    for (int i = 1; i < n; i++)
    {
        waiting[i] = waiting[i - 1] + burst[i - 1];
    }

    for (int i = 0; i < n; i++)
    {
        turnaround[i] = waiting[i] + burst[i];
    }

    printf("\nProcess\tBurst\tWaiting\tTurnaround\n");
    printf("--------------------------------------------\n");

    float total_waiting = 0;
    float total_turnaround = 0;

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\n",
               i + 1,
               burst[i],
               waiting[i],
               turnaround[i]);

        total_waiting += waiting[i];
        total_turnaround += turnaround[i];
    }

    printf("--------------------------------------------\n");

    printf("Average Waiting Time    : %.2f\n",
           total_waiting / n);

    printf("Average Turnaround Time : %.2f\n",
           total_turnaround / n);

    printf("============================================\n");
}

void round_robin_scheduling()
{
    int n, quantum;
    int burst[20], remaining[20];
    int waiting[20], turnaround[20];

    printf("\n============================================\n");
    printf("        ROUND ROBIN CPU SCHEDULING\n");
    printf("============================================\n");

    printf("Enter number of processes: ");

    if (scanf("%d", &n) != 1 || n < 1 || n > 20)
    {
        printf("Invalid process count. Enter 1-20.\n");
        return;
    }

    for (int i = 0; i < n; i++)
    {
        printf("Enter burst time for P%d: ", i + 1);

        if (scanf("%d", &burst[i]) != 1 || burst[i] <= 0)
        {
            printf("Burst time must be a positive integer.\n");
            return;
        }

        remaining[i] = burst[i];
        waiting[i] = 0;
    }

    printf("Enter time quantum: ");

    if (scanf("%d", &quantum) != 1 || quantum <= 0)
    {
        printf("Time quantum must be positive.\n");
        return;
    }

    int time = 0;
    int completed = 0;

    printf("\nExecution Order:\n");

    while (completed < n)
    {
        for (int i = 0; i < n; i++)
        {
            if (remaining[i] <= 0)
                continue;

            printf("P%d ", i + 1);

            int slice = remaining[i] < quantum
                      ? remaining[i] : quantum;

            time += slice;
            remaining[i] -= slice;

            if (remaining[i] == 0)
            {
                turnaround[i] = time;
                waiting[i] = turnaround[i] - burst[i];
                completed++;
            }
        }
    }

    printf("\n\nProcess\tBurst\tWaiting\tTurnaround\n");
    printf("--------------------------------------------\n");

    float total_waiting = 0;
    float total_turnaround = 0;

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\n",
               i + 1, burst[i], waiting[i], turnaround[i]);

        total_waiting += waiting[i];
        total_turnaround += turnaround[i];
    }

    printf("--------------------------------------------\n");
    printf("Average Waiting Time    : %.2f\n",
           total_waiting / n);
    printf("Average Turnaround Time : %.2f\n",
           total_turnaround / n);
    printf("============================================\n");
}

void priority_scheduling()
{
    int n;
    int pid[20];
    int burst[20];
    int priority[20];
    int waiting[20];
    int turnaround[20];

    printf("\n============================================\n");
    printf("         PRIORITY CPU SCHEDULING\n");
    printf("============================================\n");

    printf("Enter number of processes: ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++)
    {
        pid[i] = i + 1;

        printf("\nEnter burst time for P%d: ", pid[i]);
        scanf("%d", &burst[i]);

        printf("Enter priority for P%d: ", pid[i]);
        scanf("%d", &priority[i]);
    }

    /* Smaller number = higher priority */

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (priority[j] < priority[i])
            {
                int temp;

                temp = priority[i];
                priority[i] = priority[j];
                priority[j] = temp;

                temp = burst[i];
                burst[i] = burst[j];
                burst[j] = temp;

                temp = pid[i];
                pid[i] = pid[j];
                pid[j] = temp;
            }
        }
    }

    waiting[0] = 0;

    for (int i = 1; i < n; i++)
    {
        waiting[i] = waiting[i - 1] + burst[i - 1];
    }

    float total_waiting = 0;
    float total_turnaround = 0;

    printf("\nExecution Order:\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d ", pid[i]);
    }

    printf("\n\n");

    printf("Process\tBurst\tPriority\tWaiting\tTurnaround\n");
    printf("------------------------------------------------\n");

    for (int i = 0; i < n; i++)
    {
        turnaround[i] = waiting[i] + burst[i];

        printf("P%d\t%d\t%d\t\t%d\t%d\n",
               pid[i],
               burst[i],
               priority[i],
               waiting[i],
               turnaround[i]);

        total_waiting += waiting[i];
        total_turnaround += turnaround[i];
    }

    printf("------------------------------------------------\n");

    printf("Average Waiting Time    : %.2f\n",
           total_waiting / n);

    printf("Average Turnaround Time : %.2f\n",
           total_turnaround / n);

    printf("============================================\n");
}
void sjf_scheduling()
{
    int n;
    int pid[20];
    int burst[20];
    int waiting[20];
    int turnaround[20];

    printf("\n============================================\n");
    printf("           SJF CPU SCHEDULING\n");
    printf("============================================\n");

    printf("Enter number of processes: ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++)
    {
        pid[i] = i + 1;

        printf("Enter burst time for P%d: ", pid[i]);
        scanf("%d", &burst[i]);
    }

    /*
     * Sort processes according to
     * shortest burst time.
     */
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (burst[j] < burst[i])
            {
                int temp;

                temp = burst[i];
                burst[i] = burst[j];
                burst[j] = temp;

                temp = pid[i];
                pid[i] = pid[j];
                pid[j] = temp;
            }
        }
    }

    waiting[0] = 0;

    for (int i = 1; i < n; i++)
    {
        waiting[i] = waiting[i - 1] + burst[i - 1];
    }

    float total_waiting = 0;
    float total_turnaround = 0;

    printf("\nExecution Order:\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d ", pid[i]);
    }

    printf("\n\n");

    printf("Process\tBurst\tWaiting\tTurnaround\n");
    printf("--------------------------------------------\n");

    for (int i = 0; i < n; i++)
    {
        turnaround[i] = waiting[i] + burst[i];

        printf("P%d\t%d\t%d\t%d\n",
               pid[i],
               burst[i],
               waiting[i],
               turnaround[i]);

        total_waiting += waiting[i];
        total_turnaround += turnaround[i];
    }

    printf("--------------------------------------------\n");

    printf("Average Waiting Time    : %.2f\n",
           total_waiting / n);

    printf("Average Turnaround Time : %.2f\n",
           total_turnaround / n);

    printf("============================================\n");
}