
#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *log_file;
static char *log_path;
static size_t rotation_limit;
static size_t current_size;

static const char *level_name(LogLevel level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG:   return "DEBUG";
    case LOG_LEVEL_INFO:    return "INFO";
    case LOG_LEVEL_WARNING: return "WARNING";
    case LOG_LEVEL_ERROR:   return "ERROR";
    default:                return NULL;
    }
}

static char *copy_string(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy)
        memcpy(copy, s, n);
    return copy;
}

static int json_string(FILE *out, const char *s)
{
    if (fputc('"', out) == EOF)
        return -1;

    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        switch (*p) {
        case '"':  if (fputs("\\\"", out) == EOF) return -1; break;
        case '\\': if (fputs("\\\\", out) == EOF) return -1; break;
        case '\n': if (fputs("\\n", out) == EOF) return -1; break;
        case '\r': if (fputs("\\r", out) == EOF) return -1; break;
        case '\t': if (fputs("\\t", out) == EOF) return -1; break;
        default:
            if (*p < 0x20) {
                if (fprintf(out, "\\u%04x", (unsigned)*p) < 0)
                    return -1;
            } else if (fputc(*p, out) == EOF) {
                return -1;
            }
        }
    }

    return fputc('"', out) == EOF ? -1 : 0;
}

static int rotate_if_needed(size_t incoming)
{
    if (!rotation_limit || current_size + incoming <= rotation_limit)
        return 0;

    if (fflush(log_file) != 0)
        return -1;
    if (fclose(log_file) != 0) {
        log_file = NULL;
        return -1;
    }
    log_file = NULL;

    size_t n = strlen(log_path) + 3;
    char *old_path = malloc(n);
    if (!old_path)
        return -1;

    snprintf(old_path, n, "%s.1", log_path);
    (void)remove(old_path);

    if (rename(log_path, old_path) != 0 && errno != ENOENT) {
        free(old_path);
        return -1;
    }
    free(old_path);

    log_file = fopen(log_path, "a");
    if (!log_file)
        return -1;

    current_size = 0;
    return 0;
}

int logger_open(const char *path, size_t max_bytes)
{
    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }
    if (log_file) {
        errno = EBUSY;
        return -1;
    }

    FILE *file = fopen(path, "a");
    if (!file)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return -1;
    }

    char *saved_path = copy_string(path);
    if (!saved_path) {
        fclose(file);
        return -1;
    }

    log_file = file;
    log_path = saved_path;
    rotation_limit = max_bytes;
    current_size = (size_t)size;
    return 0;
}

int logger_event(LogLevel level, const char *event, const char *message)
{
    const char *name = level_name(level);
    if (!log_file || !event || !message || !name) {
        errno = EINVAL;
        return -1;
    }

    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    if (now == (time_t)-1 || !utc) {
        errno = EINVAL;
        return -1;
    }

    char timestamp[32];
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ",
                 utc) == 0) {
        errno = EINVAL;
        return -1;
    }

    char record[4096];
    int n = snprintf(record, sizeof(record),
                     "{\"timestamp\":\"%s\",\"level\":\"%s\",\"event\":",
                     timestamp, name);
    if (n < 0 || (size_t)n >= sizeof(record)) {
        errno = EINVAL;
        return -1;
    }

    /* Use a temporary file stream to escape JSON strings portably. */
    FILE *tmp = tmpfile();
    if (!tmp)
        return -1;

    if (json_string(tmp, event) != 0 ||
        fputs(",\"message\":", tmp) == EOF ||
        json_string(tmp, message) != 0 ||
        fputs("}\n", tmp) == EOF ||
        fflush(tmp) != 0) {
        fclose(tmp);
        return -1;
    }

    long escaped_len = ftell(tmp);
    if (escaped_len < 0 ||
        (size_t)n + (size_t)escaped_len >= sizeof(record)) {
        fclose(tmp);
        errno = EINVAL;
        return -1;
    }

    if (fseek(tmp, 0, SEEK_SET) != 0) {
        fclose(tmp);
        return -1;
    }

    size_t used = (size_t)n;
    size_t got = fread(record + used, 1, (size_t)escaped_len, tmp);
    fclose(tmp);
    if (got != (size_t)escaped_len) {
        errno = EIO;
        return -1;
    }

    used += got;
    if (rotate_if_needed(used) != 0)
        return -1;

    if (fwrite(record, 1, used, log_file) != used ||
        fflush(log_file) != 0)
        return -1;

    current_size += used;
    return 0;
}

int logger_close(void)
{
    if (!log_file) {
        errno = EINVAL;
        return -1;
    }

    int result = fflush(log_file);
    if (fclose(log_file) != 0)
        result = EOF;

    log_file = NULL;
    free(log_path);
    log_path = NULL;
    rotation_limit = 0;
    current_size = 0;

    return result == 0 ? 0 : -1;
}
