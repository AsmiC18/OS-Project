#include "aegisos_sim.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f) {
    fprintf(f,
        "AegisOS CPU-allocation simulator\n\n"
        "Usage:\n"
        "  aegisos-sim run --policy NAME --scenario NAME [--seed N] [--duration MS] [--csv FILE]\n"
        "  aegisos-sim experiment [--runs N] [--duration MS] [--out FILE]\n"
        "  aegisos-sim policies\n\n"
        "Policies: equal, fixed_high_share, foreground_priority, background_cap, adaptive\n"
        "Scenarios: steady, bursty, sustained, mixed\n");
}

static int parse_long(const char *s, long min, long max, long *out) {
    char *end = NULL;
    long v;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || !end || *end || end == s || v < min || v > max) return -1;
    *out = v;
    return 0;
}

static void print_result(const SimResult *r) {
    printf("Policy: %s\nScenario: %s\nSeed: %u\nDuration: %d ms\n",
           r->policy, r->scenario, r->seed, r->duration_ms);
    printf("Foreground requests generated/completed: %lu/%lu\n", r->foreground_requests_generated, r->foreground_requests);
    printf("Foreground mean latency: %.2f ms\n", r->foreground_mean_latency_ms);
    printf("Foreground p95 latency: %.2f ms\n", r->foreground_p95_latency_ms);
    printf("Foreground deadline-miss rate: %.2f%%\n", r->deadline_miss_rate * 100.0);
    printf("Background CPU work: %.0f ms\n", r->background_cpu_ms);
    printf("Background jobs generated/completed/on-time: %lu/%lu/%lu\n", r->background_jobs_generated, r->background_jobs_completed, r->background_jobs_on_time);
    printf("Background deadline-miss rate (resolved jobs): %.2f%%\n", r->background_deadline_miss_rate * 100.0);
    printf("CPU utilization: %.2f%%\n", r->cpu_utilization * 100.0);
    printf("Adaptive allocation changes: %lu\n", r->controller_changes);
    printf("Final foreground share: %.2f\n", r->final_foreground_share);
}

int main(int argc, char **argv) {
    char err[256];
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) { usage(stdout); return argc < 2 ? 0 : 0; }
    if (strcmp(argv[1], "--version") == 0) { puts("AegisOS simulator 1.0"); return 0; }
    if (strcmp(argv[1], "policies") == 0) {
        int i;
        puts("Policies:"); for (i = 0; i < POLICY_COUNT; i++) printf("  %s\n", policy_name((Policy)i));
        puts("Scenarios:"); for (i = 0; i < SCENARIO_COUNT; i++) printf("  %s\n", scenario_name((Scenario)i));
        return 0;
    }
    if (strcmp(argv[1], "run") == 0) {
        Policy policy = POLICY_ADAPTIVE;
        Scenario scenario = SCENARIO_BURSTY;
        uint32_t seed = 1;
        long duration = 10000, n;
        const char *csv = NULL;
        int i, have_policy = 0, have_scenario = 0;
        SimConfig cfg;
        SimResult result;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) { if (parse_policy(argv[++i], &policy)) { fprintf(stderr, "unknown policy\n"); return 2; } have_policy = 1; }
            else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) { if (parse_scenario(argv[++i], &scenario)) { fprintf(stderr, "unknown scenario\n"); return 2; } have_scenario = 1; }
            else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) { if (parse_long(argv[++i], 1, 2147483647L, &n)) { fprintf(stderr, "invalid seed\n"); return 2; } seed = (uint32_t)n; }
            else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) { if (parse_long(argv[++i], 100, 3600000, &duration)) { fprintf(stderr, "duration must be 100..3600000 ms\n"); return 2; } }
            else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv = argv[++i];
            else { usage(stderr); return 2; }
        }
        if (!have_policy || !have_scenario) { fprintf(stderr, "run requires --policy and --scenario\n"); return 2; }
        sim_config_defaults(&cfg, policy, scenario, seed); cfg.duration_ms = (int)duration;
        if (simulate(&cfg, &result, err, sizeof(err)) != 0) { fprintf(stderr, "simulation failed: %s\n", err); return 1; }
        print_result(&result);
        if (csv) { FILE *f = fopen(csv, "w"); if (!f) { perror(csv); return 1; } csv_write_header(f); csv_write_result(f, &result); fclose(f); printf("CSV written to %s\n", csv); }
        return 0;
    }
    if (strcmp(argv[1], "experiment") == 0) {
        long runs = 20, duration = 10000, n;
        const char *out = "results/raw_results.csv";
        int i;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc) { if (parse_long(argv[++i], 1, 10000, &n)) { fprintf(stderr, "invalid run count\n"); return 2; } runs = n; }
            else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) { if (parse_long(argv[++i], 100, 3600000, &n)) { fprintf(stderr, "invalid duration\n"); return 2; } duration = n; }
            else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
            else { usage(stderr); return 2; }
        }
        if (run_experiment_matrix(out, (int)runs, (int)duration, err, sizeof(err)) != 0) { fprintf(stderr, "experiment failed: %s\n", err); return 1; }
        printf("Experiment complete: %d policies x %d scenarios x %ld runs = %ld rows\n", POLICY_COUNT, SCENARIO_COUNT, runs, POLICY_COUNT * SCENARIO_COUNT * runs);
        printf("Raw results: %s\n", out);
        return 0;
    }
    usage(stderr);
    return 2;
}
