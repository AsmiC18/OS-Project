
#define _POSIX_C_SOURCE 200809L

#include "reports.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int condition, const char *message)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", message);

    if (!condition)
        failures++;
}

int main(void)
{
    MonitorSample sample;
    Anomaly anomaly;
    AegisReportData data;
    char buffer[8192];

    memset(&sample, 0, sizeof(sample));
    memset(&anomaly, 0, sizeof(anomaly));

    sample.valid = 1;
    sample.process.pid = 1234;
    snprintf(sample.process.name, sizeof(sample.process.name),
             "test,process");
    sample.timestamp.tv_sec = 100;
    sample.timestamp.tv_nsec = 500;
    sample.cpu_percent = 45.5;
    sample.memory_mb = 128.25;

    anomaly.type = ANOMALY_HIGH_CPU;
    anomaly.pid = 1234;
    anomaly.observed_value = 95.0;
    anomaly.threshold = 80.0;
    anomaly.consecutive_samples = 3;
    snprintf(anomaly.explanation, sizeof(anomaly.explanation),
             "CPU exceeded \"threshold\", sustained");

    data.samples = &sample;
    data.sample_count = 1;
    data.anomalies = &anomaly;
    data.anomaly_count = 1;

    FILE *file = tmpfile();

    check(file != NULL, "temporary report file opens");

    if (file == NULL)
        return 1;

    check(reports_write_csv(file, &data) == 0,
          "CSV report writes successfully");

    rewind(file);

    size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes] = '\0';

    check(strstr(buffer, "record_type,pid,process_name") != NULL,
          "CSV contains the expected header");

    check(strstr(buffer, "\"test,process\"") != NULL,
          "CSV escapes a process name containing a comma");

    check(strstr(buffer, "45.500,128.250") != NULL,
          "CSV contains CPU and memory measurements");

    check(strstr(buffer, "high_cpu") != NULL,
          "CSV contains the anomaly type");

    check(strstr(buffer, "\"CPU exceeded \"\"threshold\"\", sustained\"")
              != NULL,
          "CSV escapes quotes and commas in explanations");

    fclose(file);

    check(reports_write_csv(NULL, &data) == -1,
          "null output file is rejected");

    data.samples = NULL;
    data.sample_count = 1;

    check(reports_write_csv(stdout, &data) == -1,
          "missing sample data is rejected");

    printf("\nReport tests: %d failure(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
