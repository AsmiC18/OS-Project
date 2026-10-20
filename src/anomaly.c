#include "anomaly.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

void anomaly_detector_init(
    AnomalyDetector *detector,
    double cpu_threshold_percent,
    double memory_threshold_mb,
    unsigned sustained_samples)
{
    if (detector == NULL)
        return;

    memset(detector, 0, sizeof(*detector));

    detector->cpu_threshold_percent = cpu_threshold_percent;
    detector->memory_threshold_mb = memory_threshold_mb;
    detector->sustained_samples = sustained_samples;
}

static AnomalyProcessState *find_process(
    AnomalyDetector *detector,
    pid_t pid)
{
    AnomalyProcessState *free_slot = NULL;

    for (size_t i = 0; i < ANOMALY_MAX_TRACKED_PROCESSES; i++) {
        AnomalyProcessState *state = &detector->processes[i];

        if (state->active && state->pid == pid)
            return state;

        if (!state->active && free_slot == NULL)
            free_slot = state;
    }

    if (free_slot == NULL)
        return NULL;

    free_slot->pid = pid;
    free_slot->cpu_count = 0;
    free_slot->memory_count = 0;
    free_slot->cpu_alerted = 0;
    free_slot->memory_alerted = 0;
    free_slot->active = 1;

    return free_slot;
}

int anomaly_detector_evaluate(
    AnomalyDetector *detector,
    const MonitorSample *sample,
    Anomaly *out)
{
    if (detector == NULL || sample == NULL || out == NULL ||
        detector->sustained_samples == 0) {
        errno = EINVAL;
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (sample->process.pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    AnomalyProcessState *state =
        find_process(detector, sample->process.pid);

    if (state == NULL) {
        errno = ENOSPC;
        return -1;
    }

    /*
     * Invalid samples reset the counters and alert flags.
     * A later sustained anomaly can therefore trigger a new alert.
     */
    if (!sample->valid) {
        state->cpu_count = 0;
        state->memory_count = 0;
        state->cpu_alerted = 0;
        state->memory_alerted = 0;
        return 0;
    }

    /* Track consecutive high-CPU samples. */
    if (sample->cpu_percent > detector->cpu_threshold_percent) {
        state->cpu_count++;
    } else {
        state->cpu_count = 0;
        state->cpu_alerted = 0;
    }

    /* Track consecutive high-memory samples. */
    if (sample->memory_mb > detector->memory_threshold_mb) {
        state->memory_count++;
    } else {
        state->memory_count = 0;
        state->memory_alerted = 0;
    }

    /*
     * Report CPU anomalies only once per sustained episode.
     * Another alert is allowed after CPU usage returns to normal.
     */
    if (state->cpu_count >= detector->sustained_samples &&
        !state->cpu_alerted) {
        state->cpu_alerted = 1;

        out->type = ANOMALY_HIGH_CPU;
        out->pid = sample->process.pid;
        out->observed_value = sample->cpu_percent;
        out->threshold = detector->cpu_threshold_percent;
        out->consecutive_samples = state->cpu_count;

        snprintf(
            out->explanation,
            sizeof(out->explanation),
            "CPU usage remained above %.1f%% for %u samples",
            out->threshold,
            out->consecutive_samples
        );

        return 1;
    }

    /*
     * Report memory anomalies only once per sustained episode.
     * Another alert is allowed after memory usage returns to normal.
     */
    if (state->memory_count >= detector->sustained_samples &&
        !state->memory_alerted) {
        state->memory_alerted = 1;

        out->type = ANOMALY_HIGH_MEMORY;
        out->pid = sample->process.pid;
        out->observed_value = sample->memory_mb;
        out->threshold = detector->memory_threshold_mb;
        out->consecutive_samples = state->memory_count;

        snprintf(
            out->explanation,
            sizeof(out->explanation),
            "Memory usage remained above %.1f MB for %u samples",
            out->threshold,
            out->consecutive_samples
        );

        return 1;
    }

    return 0;
}
