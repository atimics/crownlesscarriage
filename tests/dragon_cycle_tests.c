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

static const CcEvent *LatestKind(const CcSim *sim, CcEventKind kind)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) return event;
    }
    return NULL;
}

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xd12a600b));
    char error[256];

    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].prosperity = 40;
    }
    CcSettlement *rich = &sim.settlements[4];
    rich->prosperity = 100;
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        sim.kingdoms[i].treasury = 300;
        sim.kingdoms[i].legitimacy = 80;
    }
    sim.kingdoms[2].treasury = 2000;
    sim.goblins.tribute_cooldown_days = 0;
    CcMoney old_hoard = sim.dragon.hoard;
    for (int32_t day = 0;
         day < 40 && sim.goblins.tributes_delivered == 0; ++day) {
        CcSimAdvanceDays(&sim, 1);
    }
    CC_CHECK(sim.goblins.tributes_delivered == 1);
    CC_CHECK(sim.goblins.last_tribute_origin_id == rich->id);
    CC_CHECK(sim.dragon.hoard > old_hoard);
    CC_CHECK(CountEvents(&sim, CC_EVENT_GOBLIN_TRIBUTE_DEPARTED) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_GOBLIN_RAID_DEPARTED) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_GOBLIN_RAIDED) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_GOBLIN_RAID_RETURNED) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_GOBLIN_TRIBUTE_DELIVERED) == 1);
    const CcEvent *delivery = LatestKind(
        &sim, CC_EVENT_GOBLIN_TRIBUTE_DELIVERED);
    CC_CHECK(delivery != NULL && delivery->parent_id != 0U);

    /* Wealth without hunger or weak commoners never launches a hoard raid. */
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].hunger = 0;
        sim.settlements[i].stock[CC_GOOD_FOOD] = 500;
        sim.settlements[i].production[CC_GOOD_FOOD] = 30;
        sim.settlements[i].consumption[CC_GOOD_FOOD] = 1;
    }
    for (int32_t i = 0; i < sim.faction_count; ++i) {
        if (sim.factions[i].kind == CC_FACTION_COMMONS) {
            sim.factions[i].support = 100;
        }
    }
    sim.hoard_raiders.cooldown_days = 0;
    CcSimAdvanceDays(&sim, 60);
    CC_CHECK(sim.dragon.retaliations == 0);
    CC_CHECK(CountEvents(&sim, CC_EVENT_DRAGON_RETALIATION) == 0);
    CC_CHECK(sim.hoard_raiders.raids_completed == 0);

    sim.player.location_id = sim.dragon.lair_settlement_id;
    sim.carriage.location_id = sim.player.location_id;
    CcMoney coins_before = sim.player.coins;
    CcCommand steal = {
        .kind = CC_COMMAND_STEAL_DRAGON_HOARD,
        .amount = 20
    };
    CC_CHECK(CcSimApply(&sim, &steal, error, sizeof(error)));
    CC_CHECK(sim.player.coins == coins_before + 20);
    CC_CHECK(sim.dragon.stolen_outstanding == 20);
    CC_CHECK(sim.dragon.omen_days_remaining == 14);
    const CcEvent *theft = LatestKind(&sim, CC_EVENT_DRAGON_HOARD_STOLEN);
    const CcEvent *omen = LatestKind(&sim, CC_EVENT_DRAGON_OMEN);
    CC_CHECK(theft != NULL && omen != NULL);
    const CcEvent *hoard_cause = CcSimEvent(&sim, theft->parent_id);
    CC_CHECK(hoard_cause != NULL &&
             hoard_cause->kind == CC_EVENT_GOBLIN_TRIBUTE_DELIVERED);
    CC_CHECK(omen->parent_id == theft->id);

    CcSimAdvanceDays(&sim, 5);
    CC_CHECK(sim.dragon.retaliations == 0);
    CcCommand partial_return = {
        .kind = CC_COMMAND_RETURN_DRAGON_TREASURE,
        .amount = 5
    };
    CC_CHECK(CcSimApply(&sim, &partial_return, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_outstanding == 15);
    CC_CHECK(sim.dragon.omen_days_remaining == 9);
    CcSimAdvanceDays(&sim, 8);
    CC_CHECK(sim.dragon.retaliations == 0);
    CcCommand give_back = {
        .kind = CC_COMMAND_RETURN_DRAGON_TREASURE,
        .amount = 15
    };
    CC_CHECK(CcSimApply(&sim, &give_back, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_outstanding == 0);
    CC_CHECK(sim.dragon.omen_days_remaining == 0);
    CcSimAdvanceDays(&sim, 40);
    CC_CHECK(sim.dragon.retaliations == 0);

    CC_CHECK(CcSimApply(&sim, &steal, error, sizeof(error)));
    CcId target_id = sim.dragon.retaliation_target_id;
    const char *path = "/tmp/crownless-dragon-cycle-tests.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(restored.dragon.stolen_outstanding == 20);
    CC_CHECK(restored.dragon.retaliation_target_id == target_id);
    CcSettlement *target = CcSimSettlementMutable(&restored, target_id);
    CC_CHECK(target != NULL);
    int32_t population_before = target->population;
    CcSimAdvanceDays(&restored, 14);
    CC_CHECK(restored.dragon.retaliations == 1);
    CC_CHECK(CountEvents(&restored, CC_EVENT_DRAGON_RETALIATION) == 1);
    CC_CHECK(target->population < population_before);
    const CcEvent *fire = LatestKind(
        &restored, CC_EVENT_DRAGON_RETALIATION);
    CC_CHECK(fire != NULL && fire->parent_id != 0U);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    CcSim unjust;
    CcSimInit(&unjust, UINT32_C(0x1ae0a117));
    for (int32_t i = 0; i < unjust.settlement_count; ++i) {
        unjust.settlements[i].prosperity = 40;
        unjust.settlements[i].hunger = 0;
        unjust.settlements[i].price[CC_GOOD_FOOD] = 4;
        unjust.settlements[i].stock[CC_GOOD_FOOD] = 500;
        unjust.kingdoms[i / 2].treasury = 120;
    }
    for (int32_t i = 0; i < unjust.faction_count; ++i) {
        if (unjust.factions[i].kind == CC_FACTION_COMMONS) {
            unjust.factions[i].support = 100;
        }
    }
    CcSettlement *unequal_town = &unjust.settlements[1];
    unequal_town->prosperity = 90;
    unequal_town->hunger = 90;
    unequal_town->price[CC_GOOD_FOOD] = 15;
    CcKingdom *unequal_kingdom = &unjust.kingdoms[0];
    unequal_kingdom->treasury = 2000;
    for (int32_t i = 0; i < unjust.faction_count; ++i) {
        if (unjust.factions[i].kingdom_id == unequal_town->kingdom_id &&
            unjust.factions[i].kind == CC_FACTION_COMMONS) {
            unjust.factions[i].support = 0;
        }
    }
    unjust.dragon.hoard = 100;
    unjust.hoard_raiders.cooldown_days = 0;
    CC_CHECK(CcSimInequalityAtSettlement(&unjust, unequal_town->id) >= 75);
    for (int32_t day = 0;
         day < 30 && unjust.dragon.stolen_outstanding == 0; ++day) {
        CcSimAdvanceDays(&unjust, 1);
    }
    CC_CHECK(unjust.dragon.stolen_outstanding > 0);
    CC_CHECK(unjust.dragon.retaliation_target_id == unequal_town->id);
    const CcEvent *social_theft = LatestKind(
        &unjust, CC_EVENT_DRAGON_HOARD_STOLEN);
    const CcEvent *social_departure = social_theft != NULL ?
        CcSimEvent(&unjust, social_theft->parent_id) : NULL;
    const CcEvent *inequality = social_departure != NULL ?
        CcSimEvent(&unjust, social_departure->parent_id) : NULL;
    CC_CHECK(social_theft != NULL &&
             social_theft->subject_id == unjust.hoard_raiders.id);
    CC_CHECK(social_departure != NULL &&
             social_departure->kind == CC_EVENT_HOARD_HEIST_DEPARTED);
    CC_CHECK(inequality != NULL &&
             inequality->kind == CC_EVENT_INEQUALITY_PRESSURE);
    const char *social_path =
        "/tmp/crownless-social-hoard-raid-tests.ccsave";
    (void)remove(social_path);
    uint64_t social_hash = CcSimHash(&unjust);
    CC_CHECK(CcSaveWrite(social_path, &unjust, error, sizeof(error)));
    CcSim social_restored;
    CC_CHECK(CcSaveRead(social_path, &social_restored,
                        error, sizeof(error)));
    CC_CHECK(CcSimHash(&social_restored) == social_hash);
    CC_CHECK(social_restored.hoard_raiders.phase ==
             CC_HOARD_RAIDERS_RETURNING);
    CC_CHECK(social_restored.dragon.theft_actor_id ==
             social_restored.hoard_raiders.id);
    unjust = social_restored;
    unequal_town = CcSimSettlementMutable(&unjust, unequal_town->id);
    CC_CHECK(unequal_town != NULL);
    CC_CHECK(remove(social_path) == 0);
    int32_t hunger_before_relief = unequal_town->hunger;
    for (int32_t day = 0;
         day < 14 && unjust.hoard_raiders.raids_completed == 0; ++day) {
        CcSimAdvanceDays(&unjust, 1);
    }
    CC_CHECK(unjust.hoard_raiders.raids_completed == 1);
    CC_CHECK(unequal_town->hunger < hunger_before_relief);
    for (int32_t day = 0;
         day < 14 && unjust.dragon.retaliations == 0; ++day) {
        CcSimAdvanceDays(&unjust, 1);
    }
    CC_CHECK(unjust.dragon.retaliations == 1);
    CC_CHECK(unjust.dragon.stolen_outstanding == 0);
    CC_CHECK(CountEvents(&unjust, CC_EVENT_HOARD_HEIST_RETURNED) == 1);
    CC_CHECK(CountEvents(&unjust, CC_EVENT_DRAGON_TREASURE_RETURNED) == 1);
    CC_CHECK(CcSimValidate(&unjust, error, sizeof(error)));

    CcSim war;
    CcSimInit(&war, UINT32_C(0x7a251e17));
    for (int32_t i = 0; i < war.settlement_count; ++i) {
        war.settlements[i].prosperity = 45;
        war.settlements[i].hunger = 0;
        war.settlements[i].stock[CC_GOOD_FOOD] = 500;
        war.settlements[i].reserve_target[CC_GOOD_FOOD] = 24;
        war.settlements[i].production[CC_GOOD_FOOD] = 30;
        war.settlements[i].consumption[CC_GOOD_FOOD] = 1;
    }
    for (int32_t i = 0; i < war.faction_count; ++i) {
        if (war.factions[i].kind == CC_FACTION_COMMONS) {
            war.factions[i].support = 100;
        }
    }
    for (int32_t i = 0; i < war.kingdom_count; ++i) {
        war.kingdoms[i].treasury = 150;
        war.kingdoms[i].legitimacy = 80;
    }
    CcSettlement *fortress = &war.settlements[2];
    CcSettlement *war_supplier = &war.settlements[3];
    CcKingdom *war_kingdom = &war.kingdoms[1];
    fortress->stock[CC_GOOD_FOOD] = 0;
    fortress->stock[CC_GOOD_TOOLS] = 0;
    fortress->reserve_target[CC_GOOD_FOOD] = 24;
    fortress->reserve_target[CC_GOOD_TOOLS] = 4;
    fortress->production[CC_GOOD_FOOD] = 0;
    fortress->consumption[CC_GOOD_FOOD] = 6;
    war_supplier->consumption[CC_GOOD_FOOD] = 10;
    war_supplier->stock[CC_GOOD_TOOLS] = 500;
    war_kingdom->treasury = 40;
    war_kingdom->legitimacy = 30;
    war.dragon.hoard = 100;
    war.hoard_raiders.cooldown_days = 0;
    CC_CHECK(CcSimWarBurdenAtSettlement(&war, fortress->id) >= 35);
    CcMoney treasury_before_funding = war_kingdom->treasury;
    CcMoney gold_before_war_supply = CcSimTrackedGold(&war);
    CcSimAdvanceDays(&war, 6);
    CC_CHECK(war_kingdom->treasury < treasury_before_funding);
    CC_CHECK(CcSimTrackedGold(&war) == gold_before_war_supply);
    CC_CHECK(CountEvents(&war, CC_EVENT_WAR_CHEST_FUNDED) >= 1);
    CC_CHECK(CountEvents(&war, CC_EVENT_WAR_SUPPLY_BOUGHT) >= 1);
    CC_CHECK(CcSimIncomingGood(&war, fortress->id, CC_GOOD_FOOD) > 0);
    CC_CHECK(war.dragon.stolen_outstanding == 0);
    CC_CHECK(war.dragon.omen_days_remaining == 0);
    for (int32_t day = 0;
         day < 30 && war.dragon.stolen_outstanding == 0; ++day) {
        CcSimAdvanceDays(&war, 1);
    }
    CC_CHECK(war.dragon.stolen_outstanding > 0);
    CC_CHECK(war.dragon.retaliation_target_id == fortress->id);
    CC_CHECK(war.hoard_raiders.motive == CC_HOARD_RAID_WAR_FINANCE);
    const CcEvent *war_theft = LatestKind(
        &war, CC_EVENT_DRAGON_HOARD_STOLEN);
    const CcEvent *war_departure = war_theft != NULL ?
        CcSimEvent(&war, war_theft->parent_id) : NULL;
    const CcEvent *war_pressure = war_departure != NULL ?
        CcSimEvent(&war, war_departure->parent_id) : NULL;
    CC_CHECK(war_theft != NULL &&
             war_theft->subject_id == war.hoard_raiders.id);
    CC_CHECK(war_departure != NULL &&
             war_departure->kind == CC_EVENT_HOARD_HEIST_DEPARTED);
    CC_CHECK(war_pressure != NULL &&
             war_pressure->kind == CC_EVENT_WAR_PRESSURE);
    CcMoney chest_before_return = fortress->war_chest;
    CcMoney gold_during_war_theft = CcSimTrackedGold(&war);
    for (int32_t day = 0;
         day < 14 && war.hoard_raiders.raids_completed == 0; ++day) {
        CcSimAdvanceDays(&war, 1);
    }
    CC_CHECK(war.hoard_raiders.raids_completed == 1);
    CC_CHECK(war.hoard_raiders.war_raids_completed == 1);
    CC_CHECK(fortress->war_chest > chest_before_return);
    CC_CHECK(CcSimTrackedGold(&war) == gold_during_war_theft);
    for (int32_t day = 0;
         day < 14 && war.dragon.retaliations == 0; ++day) {
        CcSimAdvanceDays(&war, 1);
    }
    CC_CHECK(war.dragon.retaliations == 1);
    CC_CHECK(war.dragon.stolen_outstanding == 0);
    CC_CHECK(CcSimTrackedGold(&war) == gold_during_war_theft);
    CC_CHECK(CountEvents(&war, CC_EVENT_WAR_PRESSURE) == 1);
    CC_CHECK(CountEvents(&war, CC_EVENT_DRAGON_TREASURE_RETURNED) == 1);
    CC_CHECK(CcSimValidate(&war, error, sizeof(error)));
    const char *war_path = "/tmp/crownless-war-hoard-raid-tests.ccsave";
    (void)remove(war_path);
    CC_CHECK(CcSaveWrite(war_path, &war, error, sizeof(error)));
    CcSim war_restored;
    CC_CHECK(CcSaveRead(war_path, &war_restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&war_restored) == CcSimHash(&war));
    CC_CHECK(war_restored.hoard_raiders.war_raids_completed == 1);
    CC_CHECK(remove(war_path) == 0);

    puts("Goblin tribute, war finance, and dragon retaliation tests passed");
    return 0;
}
