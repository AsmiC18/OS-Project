#include <stdio.h>
#include <signal.h>

void kill_process(int pid) {

    if (kill(pid, SIGTERM) == 0) {
        printf("Process %d terminated successfully.\n", pid);
    } else {
        perror("Failed to terminate process");
    }
}

void suspend_process(int pid) {

    if (kill(pid, SIGSTOP) == 0) {
        printf("Process %d suspended.\n", pid);
    } else {
        perror("Failed to suspend process");
    }
}

void resume_process(int pid) {

    if (kill(pid, SIGCONT) == 0) {
        printf("Process %d resumed.\n", pid);
    } else {
        perror("Failed to resume process");
    }
}
#include <sys/resource.h>
#include <errno.h>

void show_priority(int pid) {

    errno = 0;

    int priority = getpriority(PRIO_PROCESS, pid);

    if (errno != 0) {
        perror("Failed to get priority");
        return;
    }

    printf("Process %d priority: %d\n", pid, priority);
}


void set_process_priority(int pid, int priority) {

    if (setpriority(PRIO_PROCESS, pid, priority) == 0) {

        printf("Process %d priority changed to %d.\n",
               pid, priority);

    } else {

        perror("Failed to change priority");
    }
}