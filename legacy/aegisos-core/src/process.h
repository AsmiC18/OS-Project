#ifndef PROCESS_H
#define PROCESS_H

void list_processes();

void kill_process(int pid);
void suspend_process(int pid);
void resume_process(int pid);

void show_priority(int pid);
void set_process_priority(int pid, int priority);

#endif