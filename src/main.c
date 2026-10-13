#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "process.h"
#include "health.h"
#include "monitor.h"
#include "healing.h"
#include "logger.h"
#include "analysis.h"
#include "scheduler.h"
#include "ipc.h"
#include "sync.h"
#include "autoheal.h"
#define MAX_INPUT 200
#define MAX_ARGS 20
#define MAX_HISTORY 50

int main() {

    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    char history[MAX_HISTORY][MAX_INPUT];
    int history_count = 0;

    printf("====================================\n");
    printf("          AEGISOS CORE v1.0\n");
    printf(" Intelligent Linux Process Shell\n");
    printf("====================================\n");

    while (1) {

        printf("\naegis> ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        /* Store command in history */
        if (history_count < MAX_HISTORY) {
            strcpy(history[history_count], input);
            history_count++;
        }

        /* Exit */
        if (strcmp(input, "exit") == 0) {
            break;
        }

        /* Help */
        if (strcmp(input, "help") == 0) {

            printf("\nAegisOS Commands:\n");
            printf("  help      - Show available commands\n");
            printf("  cd        - Change directory\n");
            printf("  pwd       - Show current directory\n");
            printf("  clear     - Clear terminal\n");
            printf("  history   - Show command history\n");
            printf("  exit      - Exit AegisOS\n");

            continue;
        }

        /* Clear */
        if (strcmp(input, "clear") == 0) {

            system("clear");
            continue;
        }

        /* History */
        if (strcmp(input, "history") == 0) {

            for (int j = 0; j < history_count; j++) {
                printf("%d  %s\n", j + 1, history[j]);
            }

            continue;
        }

        /* Split command */
        int i = 0;

        char *token = strtok(input, " ");

        while (token != NULL && i < MAX_ARGS - 1) {

            args[i] = token;
            i++;

            token = strtok(NULL, " ");
        }

        args[i] = NULL;

        /* Built-in pwd */
        if (strcmp(args[0], "pwd") == 0) {

            char directory[500];

            getcwd(directory, sizeof(directory));

            printf("%s\n", directory);

            continue;
        }

        /* Built-in cd */
        if (strcmp(args[0], "cd") == 0) {

            if (args[1] == NULL) {

                printf("Usage: cd <directory>\n");

            } else if (chdir(args[1]) != 0) {

                perror("cd");

            }

            continue;
        }
        if (strcmp(args[0], "processes") == 0) {

    list_processes();

    continue;
}
/* Kill process */

if (strcmp(args[0], "kill") == 0) {

    if (args[1] == NULL) {
        printf("Usage: kill <PID>\n");
    } else {
        kill_process(atoi(args[1]));
    }

    continue;
}


/* Suspend process */

if (strcmp(args[0], "suspend") == 0) {

    if (args[1] == NULL) {
        printf("Usage: suspend <PID>\n");
    } else {
        suspend_process(atoi(args[1]));
    }

    continue;
}


/* Resume process */

if (strcmp(args[0], "resume") == 0) {

    if (args[1] == NULL) {
        printf("Usage: resume <PID>\n");
    } else {
        resume_process(atoi(args[1]));
    }

    continue;
}
/* Show process priority */

if (strcmp(args[0], "priority") == 0) {

    if (args[1] == NULL) {

        printf("Usage: priority <PID>\n");

    } else {

        show_priority(atoi(args[1]));
    }

    continue;
}


/* Change process priority */

if (strcmp(args[0], "setpriority") == 0) {

    if (args[1] == NULL || args[2] == NULL) {

        printf("Usage: setpriority <PID> <nice_value>\n");

    } else {

        set_process_priority(
            atoi(args[1]),
            atoi(args[2])
        );
    }

    continue;
}
/* Resource Monitor */

if (strcmp(args[0], "monitor") == 0) {

    monitor_processes();

    continue;
}
else if (strcmp(args[0], "health") == 0)
{
    show_health();
    continue;
}
else if (strcmp(args[0], "heal") == 0)
{
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL)
    {
        printf("Usage: heal <PID> <SCORE> <CPU>\n");
    }
    else
    {
        int pid = atoi(args[1]);
        int score = atoi(args[2]);
        float cpu = atof(args[3]);

        analyze_process_health(pid, score, cpu);
    }

    continue;
}
else if (strcmp(args[0], "logs") == 0)
{
    show_logs();
    continue;
}

else if (strcmp(args[0], "analyze") == 0)
{
    if (args[1] == NULL)
    {
        printf("Usage: analyze <PID>\n");
    }
    else
    {
        int pid = atoi(args[1]);

        analyze_process(pid);
    }

    continue;
}
else if (strcmp(args[0], "fcfs") == 0)
{
    fcfs_scheduling();
    continue;
}
else if (strcmp(args[0], "rr") == 0)
{
    round_robin_scheduling();
    continue;
}
else if (strcmp(args[0], "priority_schedule") == 0)
{
    priority_scheduling();
    continue;
}
else if (strcmp(args[0], "sjf") == 0)
{
    sjf_scheduling();
    continue;
}
else if (strcmp(args[0], "ipc") == 0)
{
    pipe_communication();
    continue;
}
else if (strcmp(args[0], "sync") == 0)
{
    semaphore_demo();
    continue;
}
else if (strcmp(args[0], "autoheal") == 0)
{
    auto_heal();
    continue;
}
        /* External command */

        pid_t pid = fork();

        if (pid == 0) {

            execvp(args[0], args);

            perror("Command failed");

            exit(1);
        }

        else if (pid > 0) {

            wait(NULL);
        }

        else {

            perror("fork");
        }
    }

    printf("\nAegisOS terminated.\n");

    return 0;
}