#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>


static CcBanditGroup *PrepareBlockedEncounter(CcSim *sim, uint32_t seed,
                                              char *error,
                                              size_t error_capacity)
{
    CcSimInit(sim, seed);
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

static const CcEvent *FindLootEvent(const CcSim *sim)
{
    for (int32_t i = 0; i < 24; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) break;
        if (event->kind == CC_EVENT_ENCOUNTER_LOOT) return event;
    }
    return NULL;
}

static int32_t CountPlayerTrophies(const CcSim *sim)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        if (!sim->treasures[i].destroyed &&
            sim->treasures[i].owner_id == sim->player.id &&
            strstr(sim->treasures[i].name, "Outlaw Trophy") != NULL) {
            count += 1;
        }
    }
    return count;
}

int main(void)
{
    char error[192];


    CcSim conserved;
    CcBanditGroup *guards = PrepareBlockedEncounter(
        &conserved, UINT32_C(0x100dca1e), error, sizeof(error));
    CC_CHECK(guards != NULL);
    CC_CHECK(guards->supplies > 0);
    const int32_t supplies_before = guards->supplies;
    int32_t cargo_before[CC_GOOD_COUNT];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        cargo_before[good] = conserved.player.cargo[good];
    }
    const int32_t treasures_before = conserved.treasure_count;
    const int32_t trophy_slots_before = conserved.player.treasure_cargo_slots;
    CcCommand fight = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
    CC_CHECK(CcSimApply(&conserved, &fight, error, sizeof(error)));

    int32_t taken = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        const int32_t delta = conserved.player.cargo[good] -
                              cargo_before[good];
        CC_CHECK(delta >= 0);
        if (delta > 0) {
            CC_CHECK(taken == 0);
            taken = delta;
        }
    }
    CC_CHECK(supplies_before - guards->supplies ==
             4 + taken * 10);
    const int32_t trophies = CountPlayerTrophies(&conserved);
    CC_CHECK(conserved.treasure_count - treasures_before == trophies);
    CC_CHECK(conserved.player.treasure_cargo_slots ==
             trophy_slots_before + trophies);
    for (int32_t i = 0; i < conserved.treasure_count; ++i) {
        const CcTreasure *treasure = &conserved.treasures[i];
        if (treasure->owner_id != conserved.player.id ||
            strstr(treasure->name, "Outlaw Trophy") == NULL) continue;
        CC_CHECK(treasure->appraised_value >= 2);
        CC_CHECK(treasure->appraised_value <= 12);
        CC_CHECK(treasure->location_id == conserved.player.location_id);
        CC_CHECK(treasure->gold_content == 0);
        CC_CHECK(treasure->gem_content == 0);
    }
    const CcEvent *loot_event = FindLootEvent(&conserved);
    if (taken > 0 || trophies > 0) {
        CC_CHECK(loot_event != NULL);
        CC_CHECK(loot_event->parent_id != 0U);
        CC_CHECK(loot_event->magnitude == taken ||
                 loot_event->magnitude > taken);
    }
    if (loot_event != NULL) {
        CC_CHECK(loot_event->subject_id == conserved.journey.situation_id);
        CC_CHECK(loot_event->location_id == conserved.journey.route_id);
    }


    if (trophies > 0) {
        CcSettlement *market = &conserved.settlements[0];
        market->market_coins = 200;
        const CcTreasure *trophy = NULL;
        for (int32_t i = 0; i < conserved.treasure_count; ++i) {
            if (conserved.treasures[i].owner_id == conserved.player.id &&
                strstr(conserved.treasures[i].name, "Outlaw Trophy")) {
                trophy = &conserved.treasures[i];
                break;
            }
        }
        CC_CHECK(trophy != NULL);
        CcCommand sell = {
            .kind = CC_COMMAND_SELL_TREASURE,
            .target_id = trophy->id
        };
        CC_CHECK(CcSimApply(&conserved, &sell, error, sizeof(error)));
        CC_CHECK(conserved.player.treasure_cargo_slots ==
                 trophy_slots_before);
    }


    int32_t blocked = 0;
    int32_t trophies_total = 0;
    for (uint32_t seed = 1U; seed <= 500U; ++seed) {
        CcSim rolled;
        CcBanditGroup *raiders = PrepareBlockedEncounter(
            &rolled, seed, error, sizeof(error));
        CC_CHECK(raiders != NULL);
        int32_t rolled_cargo[CC_GOOD_COUNT];
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            rolled_cargo[good] = rolled.player.cargo[good];
        }
        CcCommand defend = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
        CC_CHECK(CcSimApply(&rolled, &defend, error, sizeof(error)));
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            CC_CHECK(rolled.player.cargo[good] >= rolled_cargo[good]);
        }
        blocked += 1;
        trophies_total += CountPlayerTrophies(&rolled);
    }
    CC_CHECK(blocked == 500);
    CC_CHECK(trophies_total >= 1);
    printf("loot_tests: %d fights, %d trophies\n", blocked, trophies_total);


    CcSim empty_road;
    CcBanditGroup *collectors = PrepareBlockedEncounter(
        &empty_road, UINT32_C(0x700b100d), error, sizeof(error));
    CC_CHECK(collectors != NULL);
    collectors->route_id = 0U;
    const int32_t empty_treasures = empty_road.treasure_count;
    CcCommand clean_fight = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
    CC_CHECK(CcSimApply(&empty_road, &clean_fight, error, sizeof(error)));
    CC_CHECK(empty_road.treasure_count == empty_treasures);
    CC_CHECK(FindLootEvent(&empty_road) == NULL ||
             CountPlayerTrophies(&empty_road) == 0);

    printf("loot_tests: all checks passed\n");
    return 0;
}
