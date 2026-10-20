
#ifndef AEGISOS_ANOMALY_H
#define AEGISOS_ANOMALY_H

#include "monitor.h"

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
    double cpu_threshold_percent;
    double memory_threshold_mb;
    unsigned sustained_samples;
    unsigned cpu_count;
    unsigned memory_count;
} AnomalyDetector;

void anomaly_detector_init(AnomalyDetector *detector,
                           double cpu_threshold_percent,
                           double memory_threshold_mb,
                           unsigned sustained_samples);

int anomaly_detector_evaluate(AnomalyDetector *detector,
                              const MonitorSample *sample,
                              Anomaly *out);

#endif
