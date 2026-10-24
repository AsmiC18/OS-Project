
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

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


#define AEGIS_MAX_INPUT 200
#define MAX_ARGS 20
#define MAX_HISTORY 50

static int parse_pid(const char *text, int *pid)
{
    char *end;
    long value;

    if (text == NULL || *text == '\0')
        return 0;

    value = strtol(text, &end, 10);

    if (*end != '\0' || value <= 1 || value > INT_MAX)
        return 0;

    *pid = (int)value;
    return 1;
}

static void show_help(void)
{
    printf("\n============================================\n");
    printf("             AEGISOS COMMANDS\n");
    printf("============================================\n");

    printf("\nShell Commands:\n");
    printf("  help                 Show available commands\n");
    printf("  cd <directory>       Change directory\n");
    printf("  pwd                  Show current directory\n");
    printf("  clear                Clear terminal\n");
    printf("  history              Show command history\n");
    printf("  exit                 Exit AegisOS\n");

    printf("\nProcess Management:\n");
    printf("  processes            List processes\n");
    printf("  kill <PID>           Terminate a process\n");
    printf("  suspend <PID>        Suspend a process\n");
    printf("  resume <PID>         Resume a process\n");
    printf("  priority <PID>       Show process priority\n");
    printf("  setpriority <PID> <NICE>\n");
    printf("                       Change process priority\n");

    printf("\nMonitoring and Recovery:\n");
    printf("  monitor              Show resource usage\n");
    printf("  health               Show process health scores\n");
    printf("  heal <PID> <SCORE> <CPU>\n");
    printf("                       Run manual healing logic\n");
    printf("  healcheck <PID>      Measure health and evaluate recovery\n");
    printf("  autoheal <PID>       Adjust priority for a selected PID\n");
    printf("  analyze <PID>        Analyze a process\n");
    printf("  logs                 Show recovery logs\n");

    printf("\nScheduling Algorithms:\n");
    printf("  fcfs                 First Come First Served\n");
    printf("  sjf                  Shortest Job First\n");
    printf("  rr                   Round Robin\n");
    printf("  priority_schedule    Priority scheduling\n");

    printf("\nOS Demonstrations:\n");
    printf("  ipc                  Pipe communication\n");
    printf("  sync                 Producer-Consumer synchronization\n");

    printf("============================================\n");
}

int main(void)
{
    char input[AEGIS_MAX_INPUT];
    char *args[MAX_ARGS];
    char history[MAX_HISTORY][AEGIS_MAX_INPUT];
    int history_count = 0;

    printf("====================================\n");
    printf("          AEGISOS CORE v1.0\n");
    printf(" Intelligent Linux Process Shell\n");
    printf("====================================\n");

    while (1)
    {
        printf("\naegis> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        /* Detect and discard the remainder of an overlong input line. */
        if (strchr(input, '\n') == NULL && !feof(stdin))
        {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            printf("Input too long. Try again.\n");
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        if (input[0] == '\0')
            continue;

        if (history_count < MAX_HISTORY)
        {
            snprintf(history[history_count],
                     AEGIS_MAX_INPUT, "%s", input);
            history_count++;
        }

        if (strcmp(input, "exit") == 0)
            break;

        if (strcmp(input, "help") == 0)
        {
            show_help();
            continue;
        }

        if (strcmp(input, "clear") == 0)
        {
            system("clear");
            continue;
        }

        if (strcmp(input, "history") == 0)
        {
            for (int j = 0; j < history_count; j++)
                printf("%d  %s\n", j + 1, history[j]);

            continue;
        }

        /* Tokenize input into command and arguments. */
        int argc = 0;
        char *token = strtok(input, " \t");

        while (token != NULL && argc < MAX_ARGS - 1)
        {
            args[argc++] = token;
            token = strtok(NULL, " \t");
        }

        args[argc] = NULL;

        if (argc == 0)
            continue;

        if (strcmp(args[0], "pwd") == 0)
        {
            char directory[PATH_MAX];

            if (getcwd(directory, sizeof(directory)) != NULL)
                printf("%s\n", directory);
            else
                perror("pwd");

            continue;
        }

        if (strcmp(args[0], "cd") == 0)
        {
            if (args[1] == NULL)
                printf("Usage: cd <directory>\n");
            else if (chdir(args[1]) != 0)
                perror("cd");

            continue;
        }

        if (strcmp(args[0], "processes") == 0)
        {
            list_processes();
            continue;
        }

        if (strcmp(args[0], "kill") == 0 ||
            strcmp(args[0], "suspend") == 0 ||
            strcmp(args[0], "resume") == 0 ||
            strcmp(args[0], "priority") == 0)
        {
            int pid;

            if (args[1] == NULL || !parse_pid(args[1], &pid))
            {
                printf("Usage: %s <PID> (PID must be greater than 1)\n",
                       args[0]);
                continue;
            }

            if (strcmp(args[0], "kill") == 0)
                kill_process(pid);
            else if (strcmp(args[0], "suspend") == 0)
                suspend_process(pid);
            else if (strcmp(args[0], "resume") == 0)
                resume_process(pid);
            else
                show_priority(pid);

            continue;
        }

        if (strcmp(args[0], "setpriority") == 0)
        {
            int pid;
            char *end;
            long nice_value;

            if (args[1] == NULL || args[2] == NULL ||
                !parse_pid(args[1], &pid))
            {
                printf("Usage: setpriority <PID> <nice_value>\n");
                continue;
            }

            nice_value = strtol(args[2], &end, 10);

            if (*end != '\0' || nice_value < -20 || nice_value > 19)
            {
                printf("Nice value must be between -20 and 19.\n");
                continue;
            }

            set_process_priority(pid, (int)nice_value);
            continue;
        }

        if (strcmp(args[0], "monitor") == 0)
        {
            monitor_processes();
            continue;
        }

        if (strcmp(args[0], "health") == 0)
        {
            show_health();
            continue;
        }

        if (strcmp(args[0], "healcheck") == 0)
        {
            int pid;
            float cpu = 0.0f;
            float memory = 0.0f;

            if (args[1] == NULL || !parse_pid(args[1], &pid))
            {
                printf("Usage: healcheck <PID>\n");
                continue;
            }

            int score = measure_process_health(pid, &cpu, &memory);

            if (score < 0)
            {
                printf("Could not measure health for PID %d.\n", pid);
            }
            else
            {
                printf("\nMeasured health for PID %d\n", pid);
                printf("CPU usage : %.2f%%\n", cpu);
                printf("Memory    : %.2f%% of system RAM\n", memory);
                printf("Score     : %d/100\n", score);

                analyze_process_health(pid, score, cpu);
            }

            continue;
        }

        if (strcmp(args[0], "heal") == 0)
        {
            int pid;
            char *score_end;
            char *cpu_end;

            if (args[1] == NULL || args[2] == NULL || args[3] == NULL ||
                !parse_pid(args[1], &pid))
            {
                printf("Usage: heal <PID> <SCORE> <CPU>\n");
                continue;
            }

            long score = strtol(args[2], &score_end, 10);
            float cpu = strtof(args[3], &cpu_end);

            if (*score_end != '\0' || score < 0 || score > 100 ||
                *cpu_end != '\0' || cpu < 0.0f)
            {
                printf("Invalid score or CPU value.\n");
                continue;
            }

            analyze_process_health(pid, (int)score, cpu);
            continue;
        }

        if (strcmp(args[0], "logs") == 0)
        {
            show_logs();
            continue;
        }

        if (strcmp(args[0], "analyze") == 0)
        {
            int pid;

            if (args[1] == NULL || !parse_pid(args[1], &pid))
                printf("Usage: analyze <PID>\n");
            else
                analyze_process(pid);

            continue;
        }

        if (strcmp(args[0], "fcfs") == 0)
        {
            fcfs_scheduling();
            continue;
        }

        if (strcmp(args[0], "rr") == 0)
        {
            round_robin_scheduling();
            continue;
        }

        if (strcmp(args[0], "priority_schedule") == 0)
        {
            priority_scheduling();
            continue;
        }

        if (strcmp(args[0], "sjf") == 0)
        {
            sjf_scheduling();
            continue;
        }

        if (strcmp(args[0], "ipc") == 0)
        {
            pipe_communication();
            continue;
        }

        if (strcmp(args[0], "sync") == 0)
        {
            semaphore_demo();
            continue;
        }

        if (strcmp(args[0], "autoheal") == 0)
        {
            int pid;

            if (args[1] == NULL || !parse_pid(args[1], &pid))
                printf("Usage: autoheal <PID>\n");
            else
                auto_heal_pid(pid);

            continue;
        }

        /* Execute external programs. */
        pid_t child = fork();

        if (child == 0)
        {
            execvp(args[0], args);
            perror("Command failed");
            _exit(127);
        }
        else if (child > 0)
        {
            int status;
            while (waitpid(child, &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    perror("waitpid");
                    break;
                }
            }
        }
        else
        {
            perror("fork");
        }
    }

    printf("\nAegisOS terminated.\n");
    return 0;
}
