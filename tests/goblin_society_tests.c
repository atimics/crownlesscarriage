#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

static void StartPreparedExpedition(CcSim *sim)
{
    sim->current_day = 6;
    sim->goblins.tribute_cooldown_days = 0;
    CcSimAdvanceDays(sim, 1);
    CC_CHECK(sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING);
    CC_CHECK(sim->goblins.tribute_days_remaining >= 2);
    CC_CHECK(sim->goblins.tribute_target_id != 0U);
    CC_CHECK(CountEvents(sim, CC_EVENT_GOBLIN_RAID_PREPARED) == 1);
    CC_CHECK(CountEvents(sim, CC_EVENT_GOBLIN_RAID_DEPARTED) == 0);
}

static void CheckCivicCohesionRecovery(void)
{
    static CcSim sim;
    for (int32_t variant = 0; variant < 4; ++variant) {
        CcSimInit(&sim, 42U);
        sim.current_day = 27;
        sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
        sim.goblins.tribute_cooldown_days = 100;
        sim.goblins.members = 12;
        sim.goblins.cohesion = variant == 3 ? 100 : 49;
        sim.goblins.devotion = 60;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) sim.goblins.lair_stock[good] = 0;
        sim.goblins.lair_stock[CC_GOOD_FOOD] = variant == 1 ? 0 : 20;
        sim.goblins.lair_stock[CC_GOOD_TOOLS] = variant == 2 ? 0 : 1;
        sim.dragon.territory_stability = 0;
        for (int32_t i = 0; i < sim.kingdom_count; ++i) {
            for (int32_t j = 0; j < sim.kingdom_count; ++j) sim.diplomacy[i][j] = CC_DIPLOMACY_PEACE;
        }
        CcSimAdvanceDays(&sim, 1);
        if (variant == 0) {
            CC_CHECK(sim.goblins.cohesion == 50);
            CC_CHECK(sim.dragon.territory_stability > 0);
        } else if (variant == 1 || variant == 2) {
            CC_CHECK(sim.goblins.cohesion <= 49);
            CC_CHECK(sim.dragon.territory_stability == 0);
        } else {
            CC_CHECK(sim.goblins.cohesion == 100);
        }
    }
    CcSimInit(&sim, 42U);
    sim.goblins.cohesion = 1;
    for (int32_t month = 0; month < 50; ++month) {
        sim.current_day = month * 28 + 27;
        sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
        sim.goblins.tribute_cooldown_days = 100;
        sim.goblins.members = 12;
        sim.goblins.lair_stock[CC_GOOD_FOOD] = 20;
        sim.goblins.lair_stock[CC_GOOD_TOOLS] = 1;
        CcSimAdvanceDays(&sim, 1);
    }
    CC_CHECK(sim.goblins.cohesion >= 50);
}

int main(void)
{
    CheckCivicCohesionRecovery();
    char error[256];

    CcSim trade;
    CcSimInit(&trade, UINT32_C(0x60b11a));
    CC_CHECK(trade.goblins.cohesion == 68);
    CcSettlement *lair = CcSimSettlementMutable(
        &trade, trade.goblins.lair_settlement_id);
    CC_CHECK(lair != NULL);
    trade.player.location_id = lair->id;
    trade.carriage.location_id = lair->id;
    trade.player.cargo[CC_GOOD_FOOD] = 4;
    lair->market_coins = 200;
    CcMoney player_coins = trade.player.coins;
    CcMoney market_coins = lair->market_coins;
    int32_t lair_food = trade.goblins.lair_stock[CC_GOOD_FOOD];
    int32_t covenant = trade.goblins.devotion;
    int32_t cohesion = trade.goblins.cohesion;
    CcCommand sell_food = {
        .kind = CC_COMMAND_GOBLIN_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 4
    };
    CC_CHECK(CcSimApply(&trade, &sell_food, error, sizeof(error)));
    CC_CHECK(trade.player.cargo[CC_GOOD_FOOD] == 0);
    CC_CHECK(trade.goblins.lair_stock[CC_GOOD_FOOD] == lair_food + 4);
    CC_CHECK(trade.player.coins > player_coins);
    CC_CHECK(lair->market_coins < market_coins);
    CC_CHECK(trade.goblins.cohesion > cohesion);
    CC_CHECK(trade.goblins.devotion < covenant);
    CC_CHECK(CountEvents(&trade, CC_EVENT_GOBLIN_TRADE) == 1);
    CC_CHECK(CcSimValidate(&trade, error, sizeof(error)));

    CcSim warning;
    CcSimInit(&warning, UINT32_C(0x60b11b));
    StartPreparedExpedition(&warning);
    CcSettlement *target = CcSimSettlementMutable(
        &warning, warning.goblins.tribute_target_id);
    CC_CHECK(target != NULL);
    warning.player.location_id = target->id;
    warning.carriage.location_id = target->id;
    int32_t security = target->security;
    CcCommand warn = {.kind = CC_COMMAND_GOBLIN_WARN};
    CC_CHECK(CcSimApply(&warning, &warn, error, sizeof(error)));
    CC_CHECK(warning.goblins.target_warned);
    CC_CHECK(target->security > security);
    CC_CHECK(CountEvents(&warning, CC_EVENT_GOBLIN_TARGET_WARNED) == 1);
    CcSimAdvanceDays(&warning, 1);
    CC_CHECK(warning.goblins.tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND);
    CC_CHECK(CountEvents(&warning, CC_EVENT_GOBLIN_RAID_DEPARTED) == 1);
    CC_CHECK(CcSimValidate(&warning, error, sizeof(error)));
    const char *warning_path = "/tmp/crownless-goblin-warning.ccsave";
    (void)remove(warning_path);
    uint64_t warning_hash = CcSimHash(&warning);
    CC_CHECK(CcSaveWrite(warning_path, &warning, error, sizeof(error)));
    CcSim warning_restored;
    CC_CHECK(CcSaveRead(
        warning_path, &warning_restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&warning_restored) == warning_hash);
    CC_CHECK(warning_restored.goblins.target_warned);
    (void)remove(warning_path);

    CcSim intercept;
    CcSimInit(&intercept, UINT32_C(0x60b11c));
    StartPreparedExpedition(&intercept);
    target = CcSimSettlementMutable(
        &intercept, intercept.goblins.tribute_target_id);
    CC_CHECK(target != NULL);
    intercept.player.location_id = target->id;
    intercept.carriage.location_id = target->id;
    int32_t members = intercept.goblins.members;
    covenant = intercept.goblins.devotion;
    cohesion = intercept.goblins.cohesion;
    int32_t condition = intercept.carriage.condition;
    CcCommand stop = {.kind = CC_COMMAND_GOBLIN_INTERCEPT};
    CC_CHECK(CcSimApply(&intercept, &stop, error, sizeof(error)));
    CC_CHECK(intercept.goblins.tribute_phase == CC_GOBLIN_TRIBUTE_IDLE);
    CC_CHECK(intercept.goblins.members < members);
    CC_CHECK(intercept.goblins.devotion > covenant);
    CC_CHECK(intercept.goblins.cohesion < cohesion);
    CC_CHECK(intercept.carriage.condition < condition);
    CC_CHECK(intercept.goblins.expeditions_intercepted == 1);
    CC_CHECK(CountEvents(
        &intercept, CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED) == 1);
    CC_CHECK(CcSimValidate(&intercept, error, sizeof(error)));

    const char *seed_path = "/tmp/crownless-goblin-seed.ccsave";
    (void)remove(seed_path);
    CcSim seed;
    CcSimInit(&seed, UINT32_C(0x60b11f));
    seed.current_day = 100 * 365;
    seed.royal_trade_week = seed.current_day / 7;
    seed.dragon.slain = true;
    seed.dragon.slain_day = 2;
    seed.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    seed.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    seed.dragon.body_condition = 0;
    seed.dragon.crown_strength = 0;
    seed.dragon.afterdeath_days = 100 * 365;
    seed.goblins.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_RUMORED;
    seed.goblins.dragon_seed_days_remaining = 20 * 365;
    CC_CHECK(CcSimValidate(&seed, error, sizeof(error)));
    uint64_t seed_hash = CcSimHash(&seed);
    CC_CHECK(CcSaveWrite(seed_path, &seed, error, sizeof(error)));
    CcSim seed_restored;
    CC_CHECK(CcSaveRead(seed_path, &seed_restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&seed_restored) == seed_hash);
    CC_CHECK(seed_restored.goblins.dragon_seed_phase ==
             CC_GOBLIN_DRAGON_SEED_RUMORED);
    CC_CHECK(seed_restored.goblins.dragon_seed_days_remaining == 20 * 365);
    (void)remove(seed_path);

    const char *journal_path = "/tmp/crownless-goblin-society-journal.ccsave";
    (void)remove(journal_path);
    CcSim journal_sim;
    CcSimInit(&journal_sim, UINT32_C(0x60b11e));
    lair = CcSimSettlementMutable(
        &journal_sim, journal_sim.goblins.lair_settlement_id);
    CC_CHECK(lair != NULL);
    journal_sim.player.location_id = lair->id;
    journal_sim.carriage.location_id = lair->id;
    journal_sim.player.cargo[CC_GOOD_TOOLS] = 2;
    lair->market_coins = 200;
    CcJournal *journal = CcJournalStart(
        journal_path, &journal_sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand sell_tools = {
        .kind = CC_COMMAND_GOBLIN_TRADE,
        .good = CC_GOOD_TOOLS,
        .amount = 2
    };
    CC_CHECK(CcJournalApply(
        journal, &journal_sim, &sell_tools, error, sizeof(error)));
    uint64_t journal_hash = CcSimHash(&journal_sim);
    CC_CHECK(CcJournalClose(
        &journal, &journal_sim, error, sizeof(error)));
    CcSim journal_restored;
    CC_CHECK(CcSaveRead(
        journal_path, &journal_restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&journal_restored) == journal_hash);
    CC_CHECK(journal_restored.goblins.cohesion ==
             journal_sim.goblins.cohesion);
    (void)remove(journal_path);

    puts("Goblin society and expedition choice tests passed");
    return 0;
}
