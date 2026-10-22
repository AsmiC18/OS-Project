#ifndef AEGISOS_SIM_H
#define AEGISOS_SIM_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    POLICY_EQUAL = 0,
    POLICY_FIXED_HIGH_SHARE,
    POLICY_FOREGROUND_PRIORITY,
    POLICY_BACKGROUND_CAP,
    POLICY_ADAPTIVE,
    POLICY_COUNT
} Policy;

typedef enum {
    SCENARIO_STEADY = 0,
    SCENARIO_BURSTY,
    SCENARIO_SUSTAINED,
    SCENARIO_MIXED,
    SCENARIO_COUNT
} Scenario;

typedef struct {
    Policy policy;
    Scenario scenario;
    uint32_t seed;
    int duration_ms;
    int control_interval_ms;
    int latency_target_ms;
    double initial_foreground_share;
    double min_foreground_share;
    double max_foreground_share;
    double increase_step;
    double decrease_step;
} SimConfig;

typedef struct {
    char policy[32];
    char scenario[32];
    uint32_t seed;
    int duration_ms;
    unsigned long foreground_requests;
    unsigned long foreground_requests_generated;
    double foreground_mean_latency_ms;
    double foreground_p95_latency_ms;
    double deadline_miss_rate;
    double background_cpu_ms;
    double cpu_utilization;
    unsigned long controller_changes;
    double final_foreground_share;
    unsigned long completed_background_units;
    unsigned long background_jobs_generated;
    unsigned long background_jobs_completed;
    unsigned long background_jobs_deadline_missed;
    unsigned long background_jobs_on_time;
    double background_deadline_miss_rate;
} SimResult;

const char *policy_name(Policy p);
const char *scenario_name(Scenario s);
int parse_policy(const char *s, Policy *out);
int parse_scenario(const char *s, Scenario *out);
void sim_config_defaults(SimConfig *cfg, Policy policy, Scenario scenario, uint32_t seed);
int simulate(const SimConfig *cfg, SimResult *out, char *err, size_t errcap);
int run_experiment_matrix(const char *csv_path, int repetitions, int duration_ms, char *err, size_t errcap);
void csv_write_header(void *file);
void csv_write_result(void *file, const SimResult *r);

#endif
