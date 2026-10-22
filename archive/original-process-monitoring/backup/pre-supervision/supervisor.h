
#ifndef AEGISOS_SUPERVISOR_H
#define AEGISOS_SUPERVISOR_H

typedef struct {
    char *const *argv;
    unsigned max_restarts;
    unsigned backoff_seconds;
} SupervisorConfig;

int supervisor_run(const SupervisorConfig *config);

#endif
