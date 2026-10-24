#include "aegisos_sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_names(void) {
    Policy p; Scenario s;
    assert(parse_policy("adaptive", &p) == 0 && p == POLICY_ADAPTIVE);
    assert(parse_policy("not-a-policy", &p) != 0);
    assert(parse_scenario("bursty", &s) == 0 && s == SCENARIO_BURSTY);
    assert(parse_scenario("unknown", &s) != 0);
}

static void test_determinism(void) {
    SimConfig c; SimResult a, b; char err[256];
    sim_config_defaults(&c, POLICY_ADAPTIVE, SCENARIO_BURSTY, 123);
    c.duration_ms = 3000;
    assert(simulate(&c, &a, err, sizeof(err)) == 0);
    assert(simulate(&c, &b, err, sizeof(err)) == 0);
    assert(a.foreground_requests == b.foreground_requests);
    assert(fabs(a.foreground_p95_latency_ms - b.foreground_p95_latency_ms) < 1e-9);
    assert(fabs(a.background_cpu_ms - b.background_cpu_ms) < 1e-9);
}

static void test_metrics_bounds(void) {
    int p, s;
    for (p = 0; p < POLICY_COUNT; p++) for (s = 0; s < SCENARIO_COUNT; s++) {
        SimConfig c; SimResult r; char err[256];
        sim_config_defaults(&c, (Policy)p, (Scenario)s, 99);
        c.duration_ms = 2000;
        assert(simulate(&c, &r, err, sizeof(err)) == 0);
        assert(r.cpu_utilization >= 0.0 && r.cpu_utilization <= 1.0);
        assert(r.deadline_miss_rate >= 0.0 && r.deadline_miss_rate <= 1.0);
        assert(r.foreground_p95_latency_ms >= 0.0);
        assert(r.background_cpu_ms >= 0.0 && r.background_cpu_ms <= c.duration_ms);
        assert(r.final_foreground_share >= c.min_foreground_share - 1e-9);
        assert(r.final_foreground_share <= c.max_foreground_share + 1e-9);
    }
}


static void test_controller_and_background_jobs(void) {
    SimConfig c; SimResult r; char err[256];
    sim_config_defaults(&c, POLICY_ADAPTIVE, SCENARIO_BURSTY, 7);
    c.duration_ms = 10000;
    assert(simulate(&c, &r, err, sizeof(err)) == 0);
    assert(r.controller_changes > 0);
    assert(r.background_jobs_generated > 0);
    assert(r.background_jobs_completed > 0);
    assert(r.foreground_requests_generated >= r.foreground_requests);
    assert(r.background_jobs_on_time <= r.background_jobs_completed);
    assert(r.background_deadline_miss_rate >= 0.0 && r.background_deadline_miss_rate <= 1.0);
}

static void test_invalid_config(void) {
    SimConfig c; SimResult r; char err[256];
    sim_config_defaults(&c, POLICY_ADAPTIVE, SCENARIO_STEADY, 1);
    c.duration_ms = 0;
    assert(simulate(&c, &r, err, sizeof(err)) != 0);
}

int main(void) {
    test_names(); test_determinism(); test_metrics_bounds(); test_controller_and_background_jobs(); test_invalid_config();
    puts("test_sim: all tests passed");
    return 0;
}
