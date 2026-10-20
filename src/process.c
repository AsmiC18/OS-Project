
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

int process_read(pid_t pid, AegisProcessInfo *out)
{
    if (pid <= 0 || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    char path[64];
    int n = snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);

    if (n < 0 || (size_t)n >= sizeof(path)) {
        errno = EINVAL;
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[8192];

    if (fgets(line, sizeof(line), file) == NULL) {
        int saved_errno = ferror(file) ? EIO : EINVAL;
        fclose(file);
        errno = saved_errno;
        return -1;
    }

    if (fclose(file) != 0) {
        return -1;
    }

    /*
     * Format: PID (comm) state ...
     * The command name may contain spaces or parentheses, so locate
     * the final ')' before parsing the remaining fields.
     */
    char *left = strchr(line, '(');
    char *right = strrchr(line, ')');

    if (left == NULL || right == NULL || right <= left ||
        right[1] != ' ') {
        errno = EINVAL;
        return -1;
    }

    char *end = NULL;
    errno = 0;
    long parsed_pid = strtol(line, &end, 10);

   while (end != NULL && *end == ' ') {
    end++;
}

if (errno != 0 || end != left || parsed_pid != (long)pid) {
    errno = EINVAL;
    return -1;
}

    AegisProcessInfo info = {0};
    info.pid = pid;

    size_t name_length = (size_t)(right - left - 1);
    if (name_length >= sizeof(info.name)) {
        name_length = sizeof(info.name) - 1;
    }

    memcpy(info.name, left + 1, name_length);
    info.name[name_length] = '\0';

    /*
     * Fields after comm:
     *  3 state, 4 ppid, 5 pgrp, ...,
     * 14 utime, 15 stime, ..., 24 rss.
     */
    char *saveptr = NULL;
    char *token = strtok_r(right + 2, " \n", &saveptr);

    if (token == NULL || strlen(token) != 1) {
        errno = EINVAL;
        return -1;
    }

    info.state = token[0];

    for (int field = 4; field <= 24; ++field) {
        token = strtok_r(NULL, " \n", &saveptr);

        if (token == NULL) {
            errno = EINVAL;
            return -1;
        }

        if (field == 14 || field == 15) {
            errno = 0;
            char *number_end = NULL;
            unsigned long long value = strtoull(token, &number_end, 10);

            if (errno != 0 || number_end == token || *number_end != '\0') {
                errno = EINVAL;
                return -1;
            }

            if (field == 14) {
                info.user_ticks = (uint64_t)value;
            } else {
                info.system_ticks = (uint64_t)value;
            }
        } else if (field == 24) {
            errno = 0;
            char *number_end = NULL;
            long long value = strtoll(token, &number_end, 10);

            if (errno != 0 || number_end == token || *number_end != '\0') {
                errno = EINVAL;
                return -1;
            }

            info.resident_pages = (int64_t)value;
        }
    }

    info.total_ticks = info.user_ticks + info.system_ticks;
    *out = info;

    return 0;
}
