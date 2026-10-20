
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
    int result;

    memset(&sample, 0, sizeof(sample));
    sample.valid = 1;
    sample.process.pid = 1234;

    anomaly_detector_init(&detector, 80.0, 512.0, 3);

    sample.cpu_percent = 10.0;
    sample.memory_mb = 100.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "normal usage does not trigger an alert");

    /* CPU anomaly requires three consecutive high samples. */
    sample.cpu_percent = 95.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "first high-CPU sample does not trigger an alert");

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "second high-CPU sample does not trigger an alert");

    result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1, "third high-CPU sample triggers an alert");

    if (result == 1) {
        check(anomaly.type == ANOMALY_HIGH_CPU,
              "CPU anomaly type is correct");
        check(anomaly.pid == 1234,
              "CPU anomaly identifies the process");
    }

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "continued high CPU does not repeat the alert");

    /* Returning to normal should allow a future CPU alert. */
    sample.cpu_percent = 10.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "normal CPU usage clears the alert state");

    sample.cpu_percent = 95.0;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "new CPU episode requires three samples (first)");

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "new CPU episode requires three samples (second)");

    result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1,
          "new sustained CPU episode triggers a fresh alert");

    /* Test memory anomaly detection and duplicate suppression. */
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

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "continued high memory does not repeat the alert");

    /* Verify independent counters for two processes. */
    anomaly_detector_init(&detector, 80.0, 512.0, 3);
    sample.cpu_percent = 95.0;
    sample.memory_mb = 100.0;
    sample.process.pid = 1111;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "first CPU sample for process 1111 does not alert");

    sample.process.pid = 2222;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "first CPU sample for process 2222 does not alert");

    sample.process.pid = 1111;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "second CPU sample for process 1111 does not alert");

    sample.process.pid = 2222;

    check(anomaly_detector_evaluate(&detector, &sample, &anomaly) == 0,
          "second CPU sample for process 2222 does not alert");

    sample.process.pid = 1111;

    result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1 && anomaly.pid == 1111,
          "process 1111 alerts on its third high-CPU sample");

    sample.process.pid = 2222;

    result = anomaly_detector_evaluate(&detector, &sample, &anomaly);

    check(result == 1 && anomaly.pid == 2222,
          "process 2222 alerts on its third high-CPU sample");

    printf("\nAnomaly tests: %d failure(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
