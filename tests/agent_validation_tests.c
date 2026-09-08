#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>

static int validation_calls;
static int fail_at_call;
static bool ObservedValidate(const CcSim *sim, char *error, size_t capacity)
{
    ++validation_calls;
    if (validation_calls == fail_at_call) {
        (void)snprintf(error, capacity, "injected checkpoint failure");
        return false;
    }
    return CcSimValidate(sim, error, capacity);
}
#define CcSimValidate ObservedValidate
#define main AgentSweepMain
#include "../tools/agent_sweep.c"
#undef main
#undef CcSimValidate

int main(void)
{
    static CcSim sim, before;
    CcSimInit(&sim, 123U);
    before = sim;
    int64_t checkpoint = 366;
    CC_CHECK(ValidateAnnualCheckpoint(&sim, "control", 1, 2, 1, &checkpoint));
    CC_CHECK(validation_calls == 0 && checkpoint == 366);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcSimAdvanceDays(&sim, 365);
    before = sim;
    CC_CHECK(ValidateAnnualCheckpoint(&sim, "control", 1, 2, 1, &checkpoint));
    CC_CHECK(validation_calls == 1 && checkpoint == 731);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcSimAdvanceDays(&sim, 370);
    sim.player.coins = -1;
    before = sim;
    CC_CHECK(!ValidateAnnualCheckpoint(&sim, "agent", 1, 2, 1, &checkpoint));
    CC_CHECK(validation_calls == 2 && checkpoint == 731);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);

    char *args[] = {"crownless_agent_sweep", "--seed", "1", "--years", "1"};
    validation_calls = 0;
    CC_CHECK(AgentSweepMain(5, args) == EXIT_SUCCESS);
    /* Startup, one annual boundary, and final validation for both worlds. */
    CC_CHECK(validation_calls == 6);
    for (int call = 1; call <= 6; ++call) {
        validation_calls = 0; fail_at_call = call;
        CC_CHECK(AgentSweepMain(5, args) == EXIT_FAILURE);
        CC_CHECK(validation_calls == call);
    }
    fail_at_call = 0;
    char *too_long[] = {"crownless_agent_sweep", "--years", "2147483647"};
    CC_CHECK(AgentSweepMain(3, too_long) == EXIT_FAILURE);
    char *too_many[] = {"crownless_agent_sweep", "--seed", "2147483647", "--seeds", "2"};
    CC_CHECK(AgentSweepMain(5, too_many) == EXIT_FAILURE);
    return 0;
}
