
#include "recovery.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    AegisPolicy policy;
    RecoveryContext context = {
        .registered = 1,
        .authorized = 1,
        .restarts_used = 0,
        .seconds_since_last_action = 60
    };
    RecoveryAction action;

    policy_defaults(&policy);

    /* Alert-only mode must not restart a process. */
    policy.recovery_mode = POLICY_ALERT_ONLY;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_ALERT);

    /* Dry-run mode must not restart a process. */
    policy.recovery_mode = POLICY_DRY_RUN;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_DRY_RUN);

    /* Authorized, registered app can be restarted. */
    policy.recovery_mode = POLICY_AUTO_RESTART;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_RESTART);

    /* Unregistered app must not be restarted. */
    context.registered = 0;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_NONE);
    context.registered = 1;

    /* Unauthorized app must not be restarted. */
    context.authorized = 0;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_NONE);
    context.authorized = 1;

    /* Restart limit must be enforced. */
    context.restarts_used = policy.max_restarts;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_NONE);
    context.restarts_used = 0;

    /* Cooldown must be enforced. */
    context.seconds_since_last_action = 0;
    assert(recovery_decide(&policy, &context, &action) == 0);
    assert(action == RECOVERY_ACTION_NONE);

    /* Invalid arguments must be rejected. */
    assert(recovery_decide(NULL, &context, &action) == -1);
    assert(recovery_decide(&policy, NULL, &action) == -1);
    assert(recovery_decide(&policy, &context, NULL) == -1);

    puts("Recovery tests passed.");
    return 0;
}
