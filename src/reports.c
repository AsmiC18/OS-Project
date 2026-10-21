
#include "reports.h"

#include <errno.h>
#include <string.h>

static const char *anomaly_name(AnomalyType type)
{
    switch (type) {
    case ANOMALY_NONE:
        return "none";
    case ANOMALY_HIGH_CPU:
        return "high_cpu";
    case ANOMALY_HIGH_MEMORY:
        return "high_memory";
    case ANOMALY_PROCESS_UNAVAILABLE:
        return "process_unavailable";
    default:
        return "unknown";
    }
}

/* Escape a string according to CSV quoting rules. */
static int write_csv_string(FILE *output, const char *value)
{
    if (fputc('"', output) == EOF)
        return -1;

    for (const char *p = value; *p != '\0'; p++) {
        if (*p == '"' && fputc('"', output) == EOF)
            return -1;

        if (fputc(*p, output) == EOF)
            return -1;
    }

    return fputc('"', output) == EOF ? -1 : 0;
}

int reports_write_csv(FILE *output, const AegisReportData *data)
{
    if (output == NULL || data == NULL ||
        (data->sample_count > 0 && data->samples == NULL) ||
        (data->anomaly_count > 0 && data->anomalies == NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (fputs("record_type,pid,process_name,timestamp_seconds,"
              "timestamp_nanoseconds,cpu_percent,memory_mb,"
              "anomaly_type,observed_value,threshold,"
              "consecutive_samples,explanation\n", output) == EOF) {
        return -1;
    }

    for (size_t i = 0; i < data->sample_count; i++) {
        const MonitorSample *sample = &data->samples[i];

        if (fprintf(output, "sample,%ld,", (long)sample->process.pid) < 0)
            return -1;

        if (write_csv_string(output, sample->process.name) != 0)
            return -1;

        if (fprintf(output, ",%lld,%ld,%.3f,%.3f,,,,,\n",
                    (long long)sample->timestamp.tv_sec,
                    sample->timestamp.tv_nsec,
                    sample->cpu_percent,
                    sample->memory_mb) < 0) {
            return -1;
        }
    }

    for (size_t i = 0; i < data->anomaly_count; i++) {
        const Anomaly *anomaly = &data->anomalies[i];

        if (fprintf(output, "anomaly,%ld,,,,,,",
                    (long)anomaly->pid) < 0) {
            return -1;
        }

        if (fputs(anomaly_name(anomaly->type), output) == EOF)
            return -1;

        if (fprintf(output, ",%.3f,%.3f,%u,",
                    anomaly->observed_value,
                    anomaly->threshold,
                    anomaly->consecutive_samples) < 0) {
            return -1;
        }

        if (write_csv_string(output, anomaly->explanation) != 0)
            return -1;

        if (fputc('\n', output) == EOF)
            return -1;
    }

    if (ferror(output)) {
        errno = EIO;
        return -1;
    }

    return 0;
}
