#include "sim/cc_sim.h"
#include "test_support.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static CcSim base;
static CcSim sim;
static CcBanditGroup *PrepareBlockedEncounter(CcSim *sim, uint32_t seed, uint32_t version,
                                              char *error,
                                              size_t error_capacity)
{
    CcSimInit(sim, seed);
    sim->schema_version = version;
    sim->bandits[0].route_id = sim->routes[0].id;
    CcSituation *situation = NULL;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) {
            situation = &sim->situations[i];
            break;
        }
    }
    CC_CHECK(situation != NULL);
    situation->kind = CC_SITUATION_RELIEF_DELIVERY;
    situation->target_id = sim->settlements[1].id;
    situation->good = CC_GOOD_FOOD;
    situation->quantity = 1;
    situation->progress = 0;
    situation->reward = 20;
    situation->deadline_day = sim->current_day + 40;
    sim->player.cargo[CC_GOOD_FOOD] = 1;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = situation->id
    };
    CC_CHECK(CcSimApply(sim, &accept, error, error_capacity));
    sim->routes[0].closed = true;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, error_capacity));
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == sim->routes[0].id) {
            return &sim->bandits[i];
        }
    }
    return NULL;
}


int main(void)
{
    const uint32_t versions[] = {26,27,33,34,38,39,40,41,60};
    const CcCommand commands[] = {
        {.kind=CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT},
        {.kind=CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE},
        {.kind=CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS},
        {.kind=CC_COMMAND_WITHDRAW_ENCOUNTER,.amount=0},
        {.kind=CC_COMMAND_WITHDRAW_ENCOUNTER,.amount=1},
        {.kind=CC_COMMAND_WITHDRAW_ENCOUNTER,.amount=-1},
        {.kind=CC_COMMAND_WITHDRAW_ENCOUNTER,.amount=2}
    };
    char error[256];
    for (unsigned v=0; v<sizeof(versions)/sizeof(versions[0]); ++v)
        for (unsigned seed=1; seed<=32; ++seed) {
            CC_CHECK(PrepareBlockedEncounter(&base, seed, versions[v], error, sizeof(error)) != NULL);
            for (int variant=0; variant<8; ++variant)
                for (unsigned c=0; c<sizeof(commands)/sizeof(commands[0]); ++c) {
                    sim=base;
                    if (variant == 0 || variant == 7) sim.random_state=48U;
                    if (variant == 1) sim.player.coins=0;
                    if (variant == 2) sim.player.cargo_capacity=CcPlayerCargoUsed(&sim.player);
                    if (variant == 3) sim.bandit_count=0;
                    if (variant == 4) sim.journey.phase=CC_JOURNEY_PHASE_TRAVELLING;
                    if (variant == 5) sim.journey.origin_id=0;
                    if (variant == 6) {
                        for (int g=0; g<CC_GOOD_COUNT; ++g) sim.player.cargo[g]=20;
                        sim.player.cargo_capacity=CC_GOOD_COUNT*20+20;
                    }
                    if (variant == 7) sim.treasure_count=CC_MAX_TREASURES;
                    int before=sim.player.treasure_cargo_slots;
                    bool ok=CcSimApply(&sim,&commands[c],error,sizeof(error));
                    printf("%u %u %d %u %d %d %s %016" PRIx64 "\n",versions[v],seed,variant,c,ok,sim.player.treasure_cargo_slots-before,error,CcSimHash(&sim));
                    if (ok) {
                        CcSimAdvanceRuntimeTicks(&sim,120);
                        printf("tick %016" PRIx64 "\n",CcSimHash(&sim));
                    }
                }
        }
    return 0;
}
