
#ifndef AEGISOS_REPORTS_H
#define AEGISOS_REPORTS_H

#include "anomaly.h"
#include "monitor.h"

#include <stddef.h>
#include <stdio.h>

typedef struct {
    const MonitorSample *samples;
    size_t sample_count;

    const Anomaly *anomalies;
    size_t anomaly_count;
} AegisReportData;

/*
 * Export monitoring samples and anomaly records as CSV.
 *
 * The caller owns the FILE and must open it for writing.
 * Returns 0 on success, -1 on failure and sets errno.
 */
int reports_write_csv(FILE *output, const AegisReportData *data);

#endif
