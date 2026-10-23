#include "aegisos_sim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REQUESTS 200000
#define LATENCY_WINDOW 64
#define CAP_WINDOW_MS 100
#define PERCENTILE 0.95

typedef struct {
    int arrival_ms;
    int remaining_ms;
    int completed_ms;
    int deadline_ms;
    int deadline_missed;
} Request;

typedef struct {
    Request *items;
    size_t head;
    size_t tail;
    size_t count;
} RequestQueue;

static uint32_t rng_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int rand_range(uint32_t *state, int low, int high) {
    return low + (int)(rng_next(state) % (uint32_t)(high - low + 1));
}

const char *policy_name(Policy p) {
    static const char *names[] = {"equal", "fixed_high_share", "foreground_priority", "background_cap", "adaptive"};
    return (p >= 0 && p < POLICY_COUNT) ? names[p] : "unknown";
}

const char *scenario_name(Scenario s) {
    static const char *names[] = {"steady", "bursty", "sustained", "mixed"};
    return (s >= 0 && s < SCENARIO_COUNT) ? names[s] : "unknown";
}

int parse_policy(const char *s, Policy *out) {
    int i;
    if (!s || !out) return -1;
    for (i = 0; i < POLICY_COUNT; i++) if (strcmp(s, policy_name((Policy)i)) == 0) { *out = (Policy)i; return 0; }
    return -1;
}

int parse_scenario(const char *s, Scenario *out) {
    int i;
    if (!s || !out) return -1;
    for (i = 0; i < SCENARIO_COUNT; i++) if (strcmp(s, scenario_name((Scenario)i)) == 0) { *out = (Scenario)i; return 0; }
    return -1;
}

void sim_config_defaults(SimConfig *cfg, Policy policy, Scenario scenario, uint32_t seed) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->policy = policy;
    cfg->scenario = scenario;
    cfg->seed = seed ? seed : 1;
    cfg->duration_ms = 10000;
    cfg->control_interval_ms = 100;
    cfg->latency_target_ms = 8;
    cfg->initial_foreground_share = 0.60;
    cfg->min_foreground_share = 0.35;
    cfg->max_foreground_share = 0.90;
    cfg->increase_step = 0.10;
    cfg->decrease_step = 0.05;
}

static int q_push(RequestQueue *q, Request r) {
    if (q->count >= MAX_REQUESTS) return -1;
    q->items[q->tail] = r;
    q->tail = (q->tail + 1) % MAX_REQUESTS;
    q->count++;
    return 0;
}

static Request *q_front(RequestQueue *q) {
    return q->count ? &q->items[q->head] : NULL;
}

static Request q_pop(RequestQueue *q) {
    Request r = q->items[q->head];
    q->head = (q->head + 1) % MAX_REQUESTS;
    q->count--;
    return r;
}

static int should_arrive(Scenario s, int t, uint32_t *rng, int *service_ms) {
    int phase;
    switch (s) {
        case SCENARIO_STEADY:
            if (t % 18 == 0) { *service_ms = rand_range(rng, 2, 4); return 1; }
            return 0;
        case SCENARIO_BURSTY:
            phase = t % 1000;
            if (phase < 220) {
                if (t % 8 == 0) { *service_ms = rand_range(rng, 3, 5); return 1; }
            } else if (phase % 80 == 0) { *service_ms = rand_range(rng, 1, 3); return 1; }
            return 0;
        case SCENARIO_SUSTAINED:
            if (t % 7 == 0) { *service_ms = rand_range(rng, 2, 4); return 1; }
            return 0;
        case SCENARIO_MIXED:
            phase = t % 1600;
            if (phase < 180) {
                if (t % 8 == 0) { *service_ms = rand_range(rng, 3, 5); return 1; }
            } else if (phase < 500) {
                if (t % 30 == 0) { *service_ms = rand_range(rng, 1, 3); return 1; }
            } else if (phase < 800) {
                if (t % 18 == 0) { *service_ms = rand_range(rng, 2, 4); return 1; }
            }
            return 0;
        default: return 0;
    }
}

static int should_background_arrive(Scenario s, int t, uint32_t *rng, int *service_ms, int *deadline_ms) {
    int interval, deadline;
    switch (s) {
        case SCENARIO_STEADY: interval = 18; deadline = 70; break;
        case SCENARIO_BURSTY: interval = 10; deadline = 35; break;
        case SCENARIO_SUSTAINED: interval = 10; deadline = 40; break;
        case SCENARIO_MIXED: interval = 12; deadline = 40; break;
        default: return 0;
    }
    if (t % interval != 0) return 0;
    *service_ms = rand_range(rng, 6, 11);
    *deadline_ms = t + deadline;
    return 1;
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double percentile95(const double *values, size_t n) {
    double *copy;
    size_t idx;
    double v;
    if (!n) return 0.0;
    copy = (double *)malloc(n * sizeof(double));
    if (!copy) return 0.0;
    memcpy(copy, values, n * sizeof(double));
    qsort(copy, n, sizeof(double), compare_double);
    idx = (size_t)ceil(PERCENTILE * (double)n);
    if (idx == 0) idx = 1;
    if (idx > n) idx = n;
    v = copy[idx - 1];
    free(copy);
    return v;
}

static double recent_p95(const double *latencies, size_t n) {
    if (n > LATENCY_WINDOW) { latencies += n - LATENCY_WINDOW; n = LATENCY_WINDOW; }
    return percentile95(latencies, n);
}

int simulate(const SimConfig *cfg, SimResult *out, char *err, size_t errcap) {
    RequestQueue fg = {0}, bg = {0};
    double *latencies = NULL;
    size_t latency_count = 0, latency_cap = 0;
    uint32_t rng;
    int t, service, bg_service, bg_deadline, last_control = 0, cap_window_start = 0, bg_in_cap_window = 0;
    int contested_total = 0, contested_fg = 0;
    double fg_share, latency_sum = 0.0, bg_cpu = 0.0, busy_cpu = 0.0;
    unsigned long requests = 0, deadline_misses = 0, changes = 0, bg_units = 0;
    unsigned long bg_jobs_generated = 0, bg_jobs_completed = 0, bg_jobs_on_time = 0, bg_deadline_misses = 0;
    int target;
    if (!cfg || !out || cfg->duration_ms <= 0 || cfg->control_interval_ms <= 0 || cfg->latency_target_ms <= 0) {
        if (err && errcap) snprintf(err, errcap, "invalid configuration");
        return -1;
    }
    if (cfg->min_foreground_share <= 0.0 || cfg->max_foreground_share >= 1.0 ||
        cfg->min_foreground_share > cfg->initial_foreground_share ||
        cfg->initial_foreground_share > cfg->max_foreground_share ||
        cfg->increase_step <= 0.0 || cfg->decrease_step <= 0.0) {
        if (err && errcap) snprintf(err, errcap, "invalid allocation bounds or step sizes");
        return -1;
    }
    fg.items = (Request *)malloc(MAX_REQUESTS * sizeof(Request));
    bg.items = (Request *)malloc(MAX_REQUESTS * sizeof(Request));
    if (!fg.items || !bg.items) {
        free(fg.items); free(bg.items); free(latencies);
        if (err && errcap) snprintf(err, errcap, "out of memory allocating task queues");
        return -1;
    }
    rng = cfg->seed ? cfg->seed : 1;
    fg_share = cfg->initial_foreground_share;
    target = cfg->latency_target_ms;
    memset(out, 0, sizeof(*out));

    for (t = 0; t < cfg->duration_ms; t++) {
        Request r;
        int fg_pending, bg_pending, run_fg = 0, run_bg = 0;
        if (should_arrive(cfg->scenario, t, &rng, &service)) {
            r.arrival_ms = t; r.remaining_ms = service; r.completed_ms = -1; r.deadline_ms = -1; r.deadline_missed = 0;
            if (q_push(&fg, r) != 0) { if (err && errcap) snprintf(err, errcap, "foreground queue capacity exceeded"); free(latencies); free(fg.items); free(bg.items); return -1; }
            requests++;
        }
        if (should_background_arrive(cfg->scenario, t, &rng, &bg_service, &bg_deadline)) {
            r.arrival_ms = t; r.remaining_ms = bg_service; r.completed_ms = -1;
            r.deadline_ms = bg_deadline; r.deadline_missed = 0;
            if (q_push(&bg, r) != 0) { if (err && errcap) snprintf(err, errcap, "background queue capacity exceeded"); free(latencies); free(fg.items); free(bg.items); return -1; }
            bg_jobs_generated++;
        }
        /* Count each background deadline miss once, including jobs still waiting in the queue. */
        {
            size_t k;
            for (k = 0; k < bg.count; k++) {
                Request *job = &bg.items[(bg.head + k) % MAX_REQUESTS];
                if (!job->deadline_missed && t > job->deadline_ms) {
                    job->deadline_missed = 1;
                    bg_deadline_misses++;
                }
            }
        }
        if (t - cap_window_start >= CAP_WINDOW_MS) { cap_window_start = t; bg_in_cap_window = 0; }
        fg_pending = fg.count > 0;
        bg_pending = bg.count > 0;

        if (fg_pending && bg_pending) {
            contested_total++;
            switch (cfg->policy) {
                case POLICY_EQUAL:
                    run_fg = (contested_fg < (int)floor(0.50 * contested_total));
                    if (!run_fg) run_bg = 1;
                    break;
                case POLICY_FIXED_HIGH_SHARE:
                    run_fg = (contested_fg < (int)floor(0.80 * contested_total));
                    if (!run_fg) run_bg = 1;
                    break;
                case POLICY_FOREGROUND_PRIORITY:
                    run_fg = 1;
                    break;
                case POLICY_BACKGROUND_CAP:
                    if (bg_in_cap_window >= CAP_WINDOW_MS / 2) {
                        run_fg = 1;
                    } else {
                        run_fg = (contested_fg < (int)floor(0.50 * contested_total));
                        if (!run_fg) run_bg = 1;
                    }
                    break;
                case POLICY_ADAPTIVE:
                    run_fg = (contested_fg < (int)floor(fg_share * contested_total));
                    if (!run_fg) run_bg = 1;
                    break;
                default: free(latencies); free(fg.items); free(bg.items); if (err && errcap) snprintf(err, errcap, "unknown policy"); return -1;
            }
        } else if (fg_pending) {
            run_fg = 1;
        } else if (bg_pending) {
            if (cfg->policy == POLICY_BACKGROUND_CAP && bg_in_cap_window >= CAP_WINDOW_MS / 2) {
                run_bg = 0; /* Deliberately models a hard cap that cannot borrow idle capacity. */
            } else {
                run_bg = 1;
            }
        }

        if (run_fg && fg.count) {
            Request *front = q_front(&fg);
            front->remaining_ms--;
            busy_cpu += 1.0;
            if (fg_pending && bg_pending) contested_fg++;
            if (front->remaining_ms <= 0) {
                Request done = q_pop(&fg);
                double latency = (double)(t + 1 - done.arrival_ms);
                if (latency_count >= latency_cap) {
                    size_t newcap = latency_cap ? latency_cap * 2 : 256;
                    double *tmp = (double *)realloc(latencies, newcap * sizeof(double));
                    if (!tmp) { free(latencies); free(fg.items); free(bg.items); if (err && errcap) snprintf(err, errcap, "out of memory recording latency"); return -1; }
                    latencies = tmp; latency_cap = newcap;
                }
                latencies[latency_count++] = latency;
                latency_sum += latency;
                if (latency > target) deadline_misses++;
            }
        } else if (run_bg && bg.count) {
            Request *front = q_front(&bg);
            front->remaining_ms--;
            bg_cpu += 1.0;
            bg_units++;
            busy_cpu += 1.0;
            bg_in_cap_window++;
            if (front->remaining_ms <= 0) {
                Request done = q_pop(&bg);
                bg_jobs_completed++;
                if (!done.deadline_missed && t + 1 <= done.deadline_ms) {
                    bg_jobs_on_time++;
                } else if (!done.deadline_missed) {
                    bg_deadline_misses++;
                }
            }
        }

        if ((t + 1 - last_control) >= cfg->control_interval_ms) {
            if (cfg->policy == POLICY_ADAPTIVE) {
                double p95 = recent_p95(latencies, latency_count);
                double old_share = fg_share;
                if (latency_count > 0 && (p95 > target || fg.count > 2)) {
                    fg_share += cfg->increase_step;
                    if (fg_share > cfg->max_foreground_share) fg_share = cfg->max_foreground_share;
                } else if (latency_count > 0 && p95 < target * 0.65 && fg.count == 0) {
                    fg_share -= cfg->decrease_step;
                    if (fg_share < cfg->min_foreground_share) fg_share = cfg->min_foreground_share;
                }
                if (fabs(fg_share - old_share) > 0.000001) changes++;
            }
            last_control = t + 1;
            contested_total = 0;
            contested_fg = 0;
        }
    }

    /* Requests still queued at the end are not completed and therefore do not enter latency percentiles. */
    snprintf(out->policy, sizeof(out->policy), "%s", policy_name(cfg->policy));
    snprintf(out->scenario, sizeof(out->scenario), "%s", scenario_name(cfg->scenario));
    out->seed = cfg->seed;
    out->duration_ms = cfg->duration_ms;
    out->foreground_requests = (unsigned long)latency_count;
    out->foreground_requests_generated = requests;
    out->foreground_mean_latency_ms = latency_count ? latency_sum / (double)latency_count : 0.0;
    out->foreground_p95_latency_ms = percentile95(latencies, latency_count);
    deadline_misses += requests - (unsigned long)latency_count; /* Unfinished foreground requests miss the response-time target. */
    out->deadline_miss_rate = requests ? (double)deadline_misses / (double)requests : 0.0;
    out->background_cpu_ms = bg_cpu;
    out->cpu_utilization = busy_cpu / (double)cfg->duration_ms;
    if (out->cpu_utilization > 1.0) out->cpu_utilization = 1.0;
    out->controller_changes = changes;
    out->final_foreground_share = fg_share;
    out->completed_background_units = bg_units;
    out->background_jobs_generated = bg_jobs_generated;
    out->background_jobs_completed = bg_jobs_completed;
    out->background_jobs_deadline_missed = bg_deadline_misses;
    out->background_jobs_on_time = bg_jobs_on_time;
    out->background_deadline_miss_rate = (bg_deadline_misses + bg_jobs_on_time) ? (double)bg_deadline_misses / (double)(bg_deadline_misses + bg_jobs_on_time) : 0.0;
    free(latencies);
    free(fg.items);
    free(bg.items);
    return 0;
}

void csv_write_header(void *file) {
    FILE *f = (FILE *)file;
    fprintf(f, "policy,scenario,seed,duration_ms,generated_foreground_requests,completed_foreground_requests,foreground_mean_latency_ms,foreground_p95_latency_ms,deadline_miss_rate,background_cpu_ms,cpu_utilization,controller_changes,final_foreground_share,completed_background_units,background_jobs_generated,background_jobs_completed,background_jobs_deadline_missed,background_jobs_on_time,background_deadline_miss_rate\n");
}

void csv_write_result(void *file, const SimResult *r) {
    FILE *f = (FILE *)file;
    fprintf(f, "%s,%s,%u,%d,%lu,%lu,%.4f,%.4f,%.6f,%.0f,%.6f,%lu,%.4f,%lu,%lu,%lu,%lu,%lu,%.6f\n",
            r->policy, r->scenario, r->seed, r->duration_ms, r->foreground_requests_generated, r->foreground_requests,
            r->foreground_mean_latency_ms, r->foreground_p95_latency_ms, r->deadline_miss_rate,
            r->background_cpu_ms, r->cpu_utilization, r->controller_changes,
            r->final_foreground_share, r->completed_background_units, r->background_jobs_generated, r->background_jobs_completed, r->background_jobs_deadline_missed, r->background_jobs_on_time, r->background_deadline_miss_rate);
}

int run_experiment_matrix(const char *csv_path, int repetitions, int duration_ms, char *err, size_t errcap) {
    FILE *f;
    int p, s, rep;
    if (!csv_path || repetitions <= 0 || duration_ms <= 0) { if (err && errcap) snprintf(err, errcap, "invalid experiment arguments"); return -1; }
    f = fopen(csv_path, "w");
    if (!f) { if (err && errcap) snprintf(err, errcap, "cannot open output CSV: %s", csv_path); return -1; }
    csv_write_header(f);
    for (s = 0; s < SCENARIO_COUNT; s++) {
        for (p = 0; p < POLICY_COUNT; p++) {
            for (rep = 0; rep < repetitions; rep++) {
                SimConfig cfg;
                SimResult result;
                uint32_t seed = 1000u + (uint32_t)(s * 100000 + rep * 17 + 1);
                char localerr[256];
                sim_config_defaults(&cfg, (Policy)p, (Scenario)s, seed);
                cfg.duration_ms = duration_ms;
                if (simulate(&cfg, &result, localerr, sizeof(localerr)) != 0) {
                    fclose(f); if (err && errcap) snprintf(err, errcap, "%s", localerr); return -1;
                }
                csv_write_result(f, &result);
            }
        }
    }
    if (fclose(f) != 0) { if (err && errcap) snprintf(err, errcap, "failed closing CSV output"); return -1; }
    return 0;
}
