
#ifndef AEGISOS_POLICY_H
#define AEGISOS_POLICY_H

typedef enum {
    POLICY_ALERT_ONLY = 0,
    POLICY_DRY_RUN,
    POLICY_AUTO_RESTART
} RecoveryMode;

typedef struct {
    double cpu_threshold_percent;
    double memory_threshold_mb;
    unsigned sustained_samples;
    unsigned max_restarts;
    unsigned cooldown_seconds;
    unsigned backoff_seconds;
    RecoveryMode recovery_mode;
} AegisPolicy;

void policy_defaults(AegisPolicy *policy);
int policy_load(const char *path, AegisPolicy *policy);

#endif
