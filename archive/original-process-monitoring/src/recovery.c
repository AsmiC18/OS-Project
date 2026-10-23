
#include "recovery.h"

#include <errno.h>

int recovery_decide(const AegisPolicy *policy,
                    const RecoveryContext *context,
                    RecoveryAction *action)
{
    if (!policy || !context || !action) {
        errno = EINVAL;
        return -1;
    }

    *action = RECOVERY_ACTION_NONE;

    switch (policy->recovery_mode) {
    case POLICY_ALERT_ONLY:
        *action = RECOVERY_ACTION_ALERT;
        return 0;

    case POLICY_DRY_RUN:
        *action = RECOVERY_ACTION_DRY_RUN;
        return 0;

    case POLICY_AUTO_RESTART:
        if (!context->registered || !context->authorized)
            return 0;

        if (context->restarts_used >= policy->max_restarts)
            return 0;

        if (context->seconds_since_last_action <
            policy->cooldown_seconds)
            return 0;

        *action = RECOVERY_ACTION_RESTART;
        return 0;

    default:
        errno = EINVAL;
        return -1;
    }
}
