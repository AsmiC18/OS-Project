
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

int process_read(pid_t pid, AegisProcessInfo *out)
{
    if (pid <= 0 || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    char path[64];
    int path_length = snprintf(path, sizeof(path),
                               "/proc/%ld/stat", (long)pid);

    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        errno = EINVAL;
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
        return -1;

    char line[8192];

    if (fgets(line, sizeof(line), file) == NULL) {
        int saved_errno = ferror(file) ? EIO : EINVAL;
        fclose(file);
        errno = saved_errno;
        return -1;
    }

    if (strchr(line, '\n') == NULL && !feof(file)) {
        fclose(file);
        errno = EOVERFLOW;
        return -1;
    }

    if (fclose(file) != 0)
        return -1;

    char *left = strchr(line, '(');
    char *right = strrchr(line, ')');

    if (left == NULL || right == NULL || right <= left ||
        right[1] != ' ' || right[2] == '\0') {
        errno = EINVAL;
        return -1;
    }

    errno = 0;
    char *pid_end = NULL;
    long parsed_pid = strtol(line, &pid_end, 10);

    if (errno != 0 || pid_end == line ||
        parsed_pid != (long)pid) {
        errno = EINVAL;
        return -1;
    }

    while (*pid_end == ' ')
        pid_end++;

    if (pid_end != left) {
        errno = EINVAL;
        return -1;
    }

    AegisProcessInfo info = {0};
    info.pid = pid;

    size_t name_length = (size_t)(right - left - 1);

    if (name_length >= sizeof(info.name))
        name_length = sizeof(info.name) - 1;

    memcpy(info.name, left + 1, name_length);
    info.name[name_length] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(right + 2, " \t\n", &saveptr);

    if (token == NULL || strlen(token) != 1) {
        errno = EINVAL;
        return -1;
    }

    info.state = token[0];

    for (int field = 4; field <= 24; field++) {
        token = strtok_r(NULL, " \t\n", &saveptr);

        if (token == NULL) {
            errno = EINVAL;
            return -1;
        }

        if (field == 14 || field == 15 || field == 22) {
            errno = 0;
            char *end = NULL;
            unsigned long long value = strtoull(token, &end, 10);

            if (errno != 0 || end == token || *end != '\0') {
                errno = EINVAL;
                return -1;
            }

            if (field == 14)
                info.user_ticks = (uint64_t)value;
            else if (field == 15)
                info.system_ticks = (uint64_t)value;
            else
                info.start_ticks = (uint64_t)value;
        } else if (field == 24) {
            errno = 0;
            char *end = NULL;
            long long value = strtoll(token, &end, 10);

            if (errno != 0 || end == token || *end != '\0') {
                errno = EINVAL;
                return -1;
            }

            info.resident_pages = (int64_t)value;
        }
    }

    if (UINT64_MAX - info.user_ticks < info.system_ticks) {
        errno = EOVERFLOW;
        return -1;
    }

    info.total_ticks = info.user_ticks + info.system_ticks;
    *out = info;

    return 0;
}
