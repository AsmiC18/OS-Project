
#include "anomaly.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

void anomaly_detector_init(AnomalyDetector *detector,
                           double cpu_threshold_percent,
                           double memory_threshold_mb,
                           unsigned sustained_samples)
{
    if (!detector)
        return;

    memset(detector, 0, sizeof(*detector));

    detector->cpu_threshold_percent = cpu_threshold_percent;
    detector->memory_threshold_mb = memory_threshold_mb;
    detector->sustained_samples = sustained_samples;
}

int anomaly_detector_evaluate(AnomalyDetector *detector,
                              const MonitorSample *sample,
                              Anomaly *out)
{
    if (!detector || !sample || !out ||
        detector->sustained_samples == 0) {
        errno = EINVAL;
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (!sample->valid) {
        detector->cpu_count = 0;
        detector->memory_count = 0;
        return 0;
    }

    if (sample->cpu_percent > detector->cpu_threshold_percent)
        detector->cpu_count++;
    else
        detector->cpu_count = 0;

    if (sample->memory_mb > detector->memory_threshold_mb)
        detector->memory_count++;
    else
        detector->memory_count = 0;

    if (detector->cpu_count >= detector->sustained_samples) {
        out->type = ANOMALY_HIGH_CPU;
        out->pid = sample->process.pid;
        out->observed_value = sample->cpu_percent;
        out->threshold = detector->cpu_threshold_percent;
        out->consecutive_samples = detector->cpu_count;

        snprintf(out->explanation, sizeof(out->explanation),
                 "CPU usage remained above %.1f%% for %u samples",
                 out->threshold, out->consecutive_samples);
        return 1;
    }

    if (detector->memory_count >= detector->sustained_samples) {
        out->type = ANOMALY_HIGH_MEMORY;
        out->pid = sample->process.pid;
        out->observed_value = sample->memory_mb;
        out->threshold = detector->memory_threshold_mb;
        out->consecutive_samples = detector->memory_count;

        snprintf(out->explanation, sizeof(out->explanation),
                 "Memory usage remained above %.1f MB for %u samples",
                 out->threshold, out->consecutive_samples);
        return 1;
    }

    return 0;
}
