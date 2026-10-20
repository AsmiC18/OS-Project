
#ifndef AEGISOS_MONITOR_H
#define AEGISOS_MONITOR_H

#include "process.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define MONITOR_HISTORY_CAPACITY 256

typedef struct {
    AegisProcessInfo process;
    double cpu_percent;
    double memory_mb;
    struct timespec timestamp;
    int valid;
} MonitorSample;

typedef struct {
    pid_t pid;
    uint64_t previous_ticks;
    struct timespec previous_time;
    int initialized;
} MonitorEntry;

typedef struct {
    MonitorEntry *entries;
    size_t entry_count;
    size_t entry_capacity;

    MonitorSample history[MONITOR_HISTORY_CAPACITY];
    size_t history_count;
    size_t history_next;
} ProcessMonitor;

int monitor_init(ProcessMonitor *monitor);
void monitor_destroy(ProcessMonitor *monitor);
int monitor_add_pid(ProcessMonitor *monitor, pid_t pid);
int monitor_sample(ProcessMonitor *monitor, pid_t pid, MonitorSample *out);

#endif
