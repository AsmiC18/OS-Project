
#ifndef AEGISOS_PROCESS_H
#define AEGISOS_PROCESS_H

#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>

#define AEGISOS_PROCESS_NAME_MAX 256

typedef struct {
    pid_t pid;
    char name[AEGISOS_PROCESS_NAME_MAX];
    char state;
    uint64_t user_ticks;
    uint64_t system_ticks;
    uint64_t total_ticks;
    int64_t resident_pages;
} AegisProcessInfo;

/*
 * Read information for a Linux process.
 * Returns 0 on success, -1 on failure.
 */
int process_read(pid_t pid, AegisProcessInfo *out);

#endif
