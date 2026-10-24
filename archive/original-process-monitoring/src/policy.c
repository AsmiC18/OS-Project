
#include "policy.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void policy_defaults(AegisPolicy *policy)
{
    if (!policy)
        return;

    *policy = (AegisPolicy) {
        .cpu_threshold_percent = 80.0,
        .memory_threshold_mb = 512.0,
        .sustained_samples = 3,
        .max_restarts = 3,
        .cooldown_seconds = 30,
        .backoff_seconds = 2,
        .recovery_mode = POLICY_ALERT_ONLY
    };
}

int policy_load(const char *path, AegisPolicy *policy)
{
    if (!path || !policy) {
        errno = EINVAL;
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (!file)
        return -1;

    AegisPolicy candidate;
    policy_defaults(&candidate);

    char line[256];
    unsigned line_number = 0;
    int result = 0;

    while (fgets(line, sizeof(line), file)) {
        line_number++;

        char *p = line;
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0' || *p == '#' || *p == '\n')
            continue;

        char key[100], value[100], extra[2];

        if (sscanf(p, "%99[^=]=%99s %1s", key, value, extra) != 2) {
            result = -1;
            break;
        }

        char *end = key + strlen(key);
        while (end > key && isspace((unsigned char)end[-1]))
            *--end = '\0';

        char *v = value;
        while (isspace((unsigned char)*v))
            v++;

        errno = 0;
        char *number_end = NULL;

        if (!strcmp(key, "cpu_threshold_percent")) {
            double x = strtod(v, &number_end);
            if (errno || *number_end || x <= 0 || x > 1000) {
                result = -1; break;
            }
            candidate.cpu_threshold_percent = x;
        } else if (!strcmp(key, "memory_threshold_mb")) {
            double x = strtod(v, &number_end);
            if (errno || *number_end || x <= 0) {
                result = -1; break;
            }
            candidate.memory_threshold_mb = x;
        } else if (!strcmp(key, "sustained_samples") ||
                   !strcmp(key, "max_restarts") ||
                   !strcmp(key, "cooldown_seconds") ||
                   !strcmp(key, "backoff_seconds")) {
            unsigned long x = strtoul(v, &number_end, 10);
            if (errno || *number_end || x > 1000000) {
                result = -1; break;
            }

            if (!strcmp(key, "sustained_samples")) {
                if (!x) { result = -1; break; }
                candidate.sustained_samples = (unsigned)x;
            } else if (!strcmp(key, "max_restarts")) {
                candidate.max_restarts = (unsigned)x;
            } else if (!strcmp(key, "cooldown_seconds")) {
                candidate.cooldown_seconds = (unsigned)x;
            } else {
                candidate.backoff_seconds = (unsigned)x;
            }
        } else if (!strcmp(key, "recovery_mode")) {
            if (!strcmp(v, "alert_only"))
                candidate.recovery_mode = POLICY_ALERT_ONLY;
            else if (!strcmp(v, "dry_run"))
                candidate.recovery_mode = POLICY_DRY_RUN;
            else if (!strcmp(v, "auto_restart"))
                candidate.recovery_mode = POLICY_AUTO_RESTART;
            else {
                result = -1; break;
            }
        } else {
            result = -1;
            break;
        }
    }

    if (ferror(file))
        result = -1;

    fclose(file);

    if (result != 0) {
        errno = EINVAL;
        fprintf(stderr, "Invalid policy configuration near line %u\n",
                line_number);
        return -1;
    }

    *policy = candidate;
    return 0;
}
