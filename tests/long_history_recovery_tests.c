#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static CcSim sim, restored;
static char error[256];

static void Fresh(void)
{
    CcSimInit(&sim, 42U);
    sim.goblins.tribute_cooldown_days = 10000;
    sim.dragon.hunt_cooldown_days = 10000;
    for (int32_t r = 0; r < sim.dungeons[0].room_count; ++r)
        sim.dungeons[0].rooms[r].loot_quantity = 0;
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}

static void CheckCultRecovery(void)
{
    Fresh();
    CcSettlement *home = CcSimSettlementMutable(&sim, sim.goblins.lair_settlement_id);
    CC_CHECK(home != NULL);
    home->population = 0;
    home->prosperity = 0;
    home->security = 0;
    home->service_mask = 0;
    home->service_project = CC_SERVICE_NONE;
    home->service_project_days = 0;
    memset(sim.dragon_cult.ranks, 0, sizeof(sim.dragon_cult.ranks));
    sim.dragon_cult.devotion = 80;
    for (int32_t i = 0; i < sim.settlement_count; ++i)
        sim.settlements[i].stock[CC_GOOD_FOOD] = 1000;
    CcSimAdvanceDays(&sim, 27);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_HUMAN) == 1);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_GOBLIN) == 1);
    RoundTrip();
    CcSimAdvanceDays(&sim, 56);
    CcSimAdvanceDays(&restored, 56);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_HUMAN) >= 3);

    /* Goblin recruitment depends on the living goblin population. */
    Fresh();
    for (int32_t i = 0; i < sim.settlement_count; ++i) sim.settlements[i].population = 0;
    sim.current_day = 27;
    sim.dragon_cult.devotion = 80;
    memset(sim.dragon_cult.ranks, 0, sizeof(sim.dragon_cult.ranks));
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_HUMAN) == 0);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_GOBLIN) == 1);
}

static void CheckClanOffering(void)
{
    Fresh();
    sim.dragon.slain = true;
    sim.dragon.slain_day = 1;
    sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    sim.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    sim.dragon.body_condition = 0;
    sim.dragon.crown_strength = 0;
    sim.current_day = 100 * 365 + 1;
    sim.dragon.afterdeath_days = 100 * 365;
    sim.dragon_cult.devotion = 80;
    CcGoblinFaction *clan = &sim.goblin_politics.factions[0];
    clan->coins = 80;
    clan->gold = 2;
    CcMoney coins = CcSimTrackedGold(&sim);
    int32_t gold = CcSimTrackedGood(&sim, CC_GOOD_GOLD);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(clan->target_room == 19 && clan->carried_gold == 2);
    CC_CHECK(clan->gold == 0 && clan->coins < 80);
    CC_CHECK(sim.dragon_cult.offering_stock[CC_GOOD_GOLD] == 0);
    uint32_t flags[CC_MAX_DUNGEON_LINKS];
    for (int32_t i = 0; i < sim.dungeons[0].link_count; ++i) {
        flags[i] = sim.dungeons[0].links[i].flags;
        sim.dungeons[0].links[i].flags = 0;
    }
    int32_t room = clan->porter_room;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(clan->porter_room == room && clan->carried_gold == 2);
    RoundTrip();
    for (int32_t i = 0; i < sim.dungeons[0].link_count; ++i) {
        sim.dungeons[0].links[i].flags = flags[i];
        restored.dungeons[0].links[i].flags = flags[i];
    }
    CcSimAdvanceDays(&sim, 12);
    CcSimAdvanceDays(&restored, 12);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(sim.dragon_cult.offering_stock[CC_GOOD_GOLD] == 2);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == gold);
    CC_CHECK(CcSimTrackedGold(&sim) == coins);
}

static void CheckRaidsAndCraft(void)
{
    Fresh();
    sim.current_day = 6;
    sim.goblins.tribute_cooldown_days = 0;
    sim.goblins.lair_stock[CC_GOOD_FOOD] = 40;
    sim.goblins.lair_stock[CC_GOOD_TOOLS] = 0;
    sim.goblins.lair_stock[CC_GOOD_WEAPONS] = 0;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        for (int32_t good = CC_GOOD_IRON; good <= CC_GOOD_WEAPONS; ++good) {
            sim.settlements[i].stock[good] = 0;
            sim.settlements[i].production[good] = 0;
        }
    }
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblins.tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING);
    CC_CHECK(sim.goblins.raid_motive == CC_GOBLIN_RAID_HUNGER);
    RoundTrip();

    Fresh();
    sim.current_day = 27;
    sim.goblins.lair_stock[CC_GOOD_TOOLS] = 0;
    sim.goblins.lair_stock[CC_GOOD_WEAPONS] = 0;
    sim.goblins.lair_stock[CC_GOOD_IRON] = 5;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblins.lair_stock[CC_GOOD_TOOLS] == 1);
    CC_CHECK(sim.goblins.lair_stock[CC_GOOD_WEAPONS] == 1);
    CC_CHECK(sim.goblins.lair_stock[CC_GOOD_IRON] == 0);
}

static void CheckCampDecline(void)
{
    Fresh();
    sim.current_day = 6;
    CcBanditGroup *band = &sim.bandits[0];
    band->members = 4;
    band->supplies = 100;
    band->influence = 100;
    band->raids_completed = 1000;
    band->camp_size = CC_BANDIT_OUTLAW_TOWN;
    const CcServiceKind services[] = {CC_SERVICE_BLACK_MARKET, CC_SERVICE_STABLE,
        CC_SERVICE_SMITHY, CC_SERVICE_HEALER, CC_SERVICE_BARRACKS, CC_SERVICE_MARKET,
        CC_SERVICE_GRANARY, CC_SERVICE_GUILDHALL};
    band->service_mask = 0;
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i)
        band->service_mask |= UINT32_C(1) << (uint32_t)services[i];
    for (int32_t i = 0; i < sim.settlement_count; ++i) sim.settlements[i].hunger = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(band->camp_size == CC_BANDIT_HIDEOUT);
    RoundTrip();
    band->members = 100;
    sim.current_day = 13;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(band->camp_size == CC_BANDIT_OUTLAW_TOWN);
    RoundTrip();
}

static void CheckBanditScouts(void)
{
    Fresh();
    CcBanditGroup *band = &sim.bandits[0];
    band->route_id = sim.routes[0].id;
    CcSettlement *empty = CcSimSettlementMutable(&sim, sim.routes[0].from_id);
    CcSettlement *stocked = CcSimSettlementMutable(&sim, sim.routes[0].to_id);
    CC_CHECK(empty != NULL && stocked != NULL);
    memset(empty->stock, 0, sizeof(empty->stock));
    memset(stocked->stock, 0, sizeof(stocked->stock));
    empty->security = 0;
    stocked->security = 100;
    stocked->stock[CC_GOOD_WOOD] = 8;
    CC_CHECK(CcSimLaunchBanditRaid(&sim, band->id, error, sizeof(error)));
    CC_CHECK(band->raid_target_id == stocked->id);

    /* An empty town's hunger flag contributes the same recruitment as zero. */
    empty->population = 0;
    empty->hunger = 100;
    empty->prosperity = 0;
    empty->service_mask = 0;
    empty->service_project = CC_SERVICE_NONE;
    empty->service_project_days = 0;
    sim.current_day = 6;
    band->raid_days_remaining = 10;
    restored = sim;
    CcSimSettlementMutable(&restored, empty->id)->hunger = 0;
    CcSimAdvanceDays(&sim, 1);
    CcSimAdvanceDays(&restored, 1);
    CC_CHECK(sim.bandits[0].members == restored.bandits[0].members);
}

static void CheckEmptyCarriage(void)
{
    Fresh();
    sim.current_day = 30;
    CcRoyalCarriage *carriage = &sim.royal_carriages[0];
    carriage->mode = CC_ROYAL_CARRIAGE_BLOCKED;
    carriage->active_shipment_id = 0;
    carriage->blocked_since_day = 1;
    carriage->route_id = sim.routes[0].id;
    carriage->target_id = sim.routes[0].to_id;
    carriage->destination_id = carriage->target_id;
    CcSimAdvanceDays(&sim, 1);
    bool parked = false;
    for (int32_t i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event->day != sim.current_day || event->subject_id != carriage->id) continue;
        CC_CHECK(event->kind != CC_EVENT_SHIPMENT_ARRIVED);
        if (strstr(event->text, "empty royal carriage parks") != NULL) parked = true;
    }
    CC_CHECK(parked);
    CC_CHECK(carriage->blocked_since_day != 1);
}

static void CheckReturnedCoins(void)
{
    Fresh();
    sim.dragon.theft_actor_id = sim.hoard_raiders.id;
    sim.dragon.retaliation_target_id = sim.settlements[0].id;
    sim.dragon.stolen_outstanding = 7;
    sim.dragon.omen_days_remaining = 14;
    CcGoblinFaction *clan = &sim.goblin_politics.factions[0];
    clan->porter_room = 19;
    clan->target_room = 19;
    clan->carried_coins = 5;
    CcMoney coins = CcSimTrackedGold(&sim);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.stolen_outstanding == 2);
    CC_CHECK(CcSimTrackedGold(&sim) == coins);
    clan->porter_room = 19;
    clan->target_room = 19;
    clan->carried_coins = 4;
    coins = CcSimTrackedGold(&sim);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.stolen_outstanding == 0);
    CC_CHECK(sim.dragon.theft_actor_id == 0);
    CC_CHECK(sim.dragon.omen_days_remaining == 0);
    CC_CHECK(CcSimTrackedGold(&sim) == coins);
    RoundTrip();
}

static void CheckGiftLimit(void)
{
    Fresh();
    sim.current_day = 27;
    sim.dragon.slain = true;
    sim.dragon.slain_day = 1;
    sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    sim.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    sim.dragon_cult.offering_coins = 120;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon_cult.offering_coins == 120);
}

static void CheckLegacyHistory(void)
{
    /* Captured from main 615031d9, schema 113, raw seed 42, 364 days.
       Old journals replay their original rules before the runtime upgrade. */
    CcSimInit(&sim, 42U);
    sim.schema_version = 113U;
    CcSimAdvanceDays(&sim, 364);
    CC_CHECK(CcSimHash(&sim) == UINT64_C(5945950365194471743));
}

int main(void)
{
    CheckCultRecovery();
    CheckClanOffering();
    CheckRaidsAndCraft();
    CheckCampDecline();
    CheckBanditScouts();
    CheckEmptyCarriage();
    CheckReturnedCoins();
    CheckGiftLimit();
    CheckLegacyHistory();
    puts("Cult recovery, clan gifts, raid plans, camp decline and carriage events passed.");
    return 0;
}
