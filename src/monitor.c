
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static double elapsed_seconds(struct timespec a, struct timespec b)
{
    return (double)(b.tv_sec - a.tv_sec) +
           (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

int monitor_init(ProcessMonitor *monitor)
{
    if (!monitor) {
        errno = EINVAL;
        return -1;
    }

    memset(monitor, 0, sizeof(*monitor));
    return 0;
}

void monitor_destroy(ProcessMonitor *monitor)
{
    if (!monitor)
        return;

    free(monitor->entries);
    memset(monitor, 0, sizeof(*monitor));
}

int monitor_add_pid(ProcessMonitor *monitor, pid_t pid)
{
    if (!monitor || pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < monitor->entry_count; i++) {
        if (monitor->entries[i].pid == pid)
            return 0;
    }

    if (monitor->entry_count == monitor->entry_capacity) {
        size_t capacity = monitor->entry_capacity
                        ? monitor->entry_capacity * 2 : 8;

        MonitorEntry *entries = realloc(
            monitor->entries, capacity * sizeof(*entries));

        if (!entries)
            return -1;

        monitor->entries = entries;
        monitor->entry_capacity = capacity;
    }

    MonitorEntry *entry = &monitor->entries[monitor->entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->pid = pid;

    return 0;
}

int monitor_sample(ProcessMonitor *monitor, pid_t pid, MonitorSample *out)
{
    if (!monitor || !out || pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    size_t index = monitor->entry_count;

    for (size_t i = 0; i < monitor->entry_count; i++) {
        if (monitor->entries[i].pid == pid) {
            index = i;
            break;
        }
    }

    if (index == monitor->entry_count) {
        if (monitor_add_pid(monitor, pid) != 0)
            return -1;
        index = monitor->entry_count - 1;
    }

    MonitorEntry *entry = &monitor->entries[index];
    MonitorSample sample = {0};

    if (process_read(pid, &sample.process) != 0) {
        entry->initialized = 0;
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &sample.timestamp) != 0)
        return -1;

    long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        errno = EINVAL;
        return -1;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        errno = EINVAL;
        return -1;
    }

    sample.memory_mb =
        (double)sample.process.resident_pages * (double)page_size /
        (1024.0 * 1024.0);

    if (entry->initialized) {
        double elapsed = elapsed_seconds(
            entry->previous_time, sample.timestamp);

        if (elapsed > 0.0) {
            uint64_t delta = sample.process.total_ticks >=
                             entry->previous_ticks
                           ? sample.process.total_ticks -
                             entry->previous_ticks : 0;

            /* CPU percentage is normalized to one logical CPU (0–100% per core). */
            sample.cpu_percent =
                100.0 * (double)delta /
                ((double)ticks_per_second * elapsed);
        }
    }

    entry->previous_ticks = sample.process.total_ticks;
    entry->previous_time = sample.timestamp;
    entry->initialized = 1;

    sample.valid = 1;

    monitor->history[monitor->history_next] = sample;
    monitor->history_next =
        (monitor->history_next + 1) % MONITOR_HISTORY_CAPACITY;

    if (monitor->history_count < MONITOR_HISTORY_CAPACITY)
        monitor->history_count++;

    *out = sample;
    return 0;
}
