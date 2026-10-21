#ifndef AEGISOS_ANOMALY_H
#define AEGISOS_ANOMALY_H

#include "monitor.h"

#define ANOMALY_MAX_TRACKED_PROCESSES 256

typedef enum {
    ANOMALY_NONE = 0,
    ANOMALY_HIGH_CPU,
    ANOMALY_HIGH_MEMORY,
    ANOMALY_PROCESS_UNAVAILABLE
} AnomalyType;

typedef struct {
    AnomalyType type;
    pid_t pid;
    double observed_value;
    double threshold;
    unsigned consecutive_samples;
    char explanation[160];
} Anomaly;

typedef struct {
    pid_t pid;
    unsigned cpu_count;
    unsigned memory_count;
    int cpu_alerted;
    int memory_alerted;
    int active;
} AnomalyProcessState;

typedef struct {
    double cpu_threshold_percent;
    double memory_threshold_mb;
    unsigned sustained_samples;

    AnomalyProcessState processes[ANOMALY_MAX_TRACKED_PROCESSES];
} AnomalyDetector;

void anomaly_detector_init(
    AnomalyDetector *detector,
    double cpu_threshold_percent,
    double memory_threshold_mb,
    unsigned sustained_samples
);

int anomaly_detector_evaluate(
    AnomalyDetector *detector,
    const MonitorSample *sample,
    Anomaly *out
);

#endif
