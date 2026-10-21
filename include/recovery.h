
#ifndef AEGISOS_RECOVERY_H
#define AEGISOS_RECOVERY_H

#include "policy.h"

typedef enum {
    RECOVERY_ACTION_NONE = 0,
    RECOVERY_ACTION_ALERT,
    RECOVERY_ACTION_DRY_RUN,
    RECOVERY_ACTION_RESTART
} RecoveryAction;

typedef struct {
    int registered;
    int authorized;
    unsigned restarts_used;
    unsigned seconds_since_last_action;
} RecoveryContext;

/*
 * Determine the permitted recovery action.
 * Returns -1 for invalid arguments, otherwise 0.
 * The selected action is written to *action.
 */
int recovery_decide(const AegisPolicy *policy,
                    const RecoveryContext *context,
                    RecoveryAction *action);

#endif
