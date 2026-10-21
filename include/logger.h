
#ifndef AEGISOS_LOGGER_H
#define AEGISOS_LOGGER_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} LogLevel;

/*
 * Open an append-only JSONL log.
 * max_bytes sets the rotation threshold; 0 disables rotation.
 * Returns 0 on success, -1 on failure and sets errno.
 */
int logger_open(const char *path, size_t max_bytes);

/* Write one timestamped JSONL event. */
int logger_event(LogLevel level, const char *event, const char *message);

/* Flush pending data and close the log. */
int logger_close(void);

#endif
