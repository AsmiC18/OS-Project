
#include "anomaly.h"

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
    AnomalyDetector detector;
    Anomaly anomaly;
    MonitorSample sample;

    memset(&sample, 0, sizeof(sample));
    sample.valid = 1;
    sample.process.pid = 1234;

    anomaly_detector_init(&detector, 80.0, 512.0, 3);

    sample.cpu_percent = 10.0;
    sample.memory_mb = 100.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "normal usage does not trigger an alert");

    sample.cpu_percent = 95.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "first high-CPU sample does not trigger an alert");

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "second high-CPU sample does not trigger an alert");

    int result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1, "third high-CPU sample triggers an alert");

    if (result == 1) {
        check(anomaly.type == ANOMALY_HIGH_CPU,
              "CPU anomaly type is correct");
        check(anomaly.pid == 1234,
              "CPU anomaly identifies the process");
    }

    anomaly_detector_init(&detector, 80.0, 512.0, 3);
    sample.cpu_percent = 0.0;
    sample.memory_mb = 600.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "first high-memory sample does not trigger an alert");

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "second high-memory sample does not trigger an alert");

    result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1, "third high-memory sample triggers an alert");

    if (result == 1) {
        check(anomaly.type == ANOMALY_HIGH_MEMORY,
              "memory anomaly type is correct");
    }

    printf("\nAnomaly tests: %d failure(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
