#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

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

static void FillEventLedger(CcSim *sim)
{
    while (sim->event_count < CC_MAX_EVENTS) {
        CcEvent *event = &sim->events[sim->event_count];
        *event = (CcEvent){
            .id = CcMakeId(CC_ENTITY_EVENT, sim->next_entity_serial++),
            .day = sim->current_day,
            .kind = CC_EVENT_HARVEST_FAILED,
            .subject_id = sim->settlements[0].id,
            .location_id = sim->settlements[0].id
        };
        (void)snprintf(event->text, sizeof(event->text),
                       "Ledger boundary event %d.", sim->event_count);
        sim->event_count += 1;
        sim->event_write_index = sim->event_count % CC_MAX_EVENTS;
    }
}

static void CheckFullLedgerDragonTheft(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x1297e157));
    FillEventLedger(&sim);

    CcSettlement *origin = &sim.settlements[0];
    sim.hoard_raiders.phase = CC_HOARD_RAIDERS_OUTBOUND;
    sim.hoard_raiders.motive = CC_HOARD_RAID_SOCIAL_RELIEF;
    sim.hoard_raiders.origin_settlement_id = origin->id;
    sim.hoard_raiders.cause_event_id = 0U;
    sim.hoard_raiders.days_remaining = 1;
    sim.dragon.hoard = 100;
    sim.dragon.stolen_outstanding = 0;
    sim.goblins.members = 12;
    sim.goblins.devotion = 0;
    sim.goblins.hoard_defenses = 0;

    CcSimAdvanceDays(&sim, 1);

    const CcEvent *theft = CcSimEvent(
        &sim, sim.hoard_raiders.cause_event_id);
    const CcEvent *omen = CcSimEvent(&sim, sim.dragon.omen_event_id);
    CC_CHECK(theft != NULL);
    CC_CHECK(theft->kind == CC_EVENT_DRAGON_HOARD_STOLEN);
    CC_CHECK(omen != NULL);
    CC_CHECK(omen->kind == CC_EVENT_DRAGON_OMEN);
    CC_CHECK(omen->parent_id == theft->id);
    char error[256];
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckFullLedgerDragonSlaying(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x12951a17));
    FillEventLedger(&sim);

    CcSettlement *origin = &sim.settlements[0];
    sim.dragon_campaign.phase = CC_DRAGON_CAMPAIGN_OUTBOUND;
    sim.dragon_campaign.pledged_kingdom_mask = 1U;
    sim.dragon_campaign.alliance_kingdom_mask = 7U;
    sim.dragon_campaign.origin_settlement_id = origin->id;
    sim.dragon_campaign.cause_event_id = 0U;
    sim.dragon_campaign.days_remaining = 1;
    sim.dragon_campaign.attempts = 1;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim.dragon_campaign.supplies[good] = 400;
    }
    sim.goblins.members = 12;
    sim.goblins.devotion = 0;
    sim.goblins.cohesion = 0;
    sim.goblins.hoard_defenses = 0;
    sim.dragon.body_condition = 1;
    sim.dragon.crown_strength = 0;
    sim.dragon.memory_integrity = 0;
    sim.dragon.territory_stability = 0;
    if (sim.treasure_count >= CC_MAX_TREASURES) {
        sim.treasures[sim.treasure_count - 1].destroyed = true;
    }

    CcSimAdvanceDays(&sim, 1);

    const CcEvent *slaying = CcSimEvent(
        &sim, sim.dragon_campaign.cause_event_id);
    CC_CHECK(slaying != NULL);
    CC_CHECK(slaying->kind == CC_EVENT_DRAGON_SLAIN);
    CC_CHECK(sim.dragon.lifecycle_event_id == slaying->id);

    const CcTreasure *bane = NULL;
    for (int32_t i = 0; i < sim.treasure_count; ++i) {
        if (!sim.treasures[i].destroyed &&
            strncmp(sim.treasures[i].name, "Bane of ", 8) == 0) {
            bane = &sim.treasures[i];
            break;
        }
    }
    CC_CHECK(bane != NULL);
    const CcEvent *forging = NULL;
    for (int32_t i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event != NULL && event->kind == CC_EVENT_TREASURE_CRAFTED &&
            event->subject_id == bane->id) {
            forging = event;
            break;
        }
    }
    CC_CHECK(forging != NULL);
    CC_CHECK(forging->parent_id == slaying->id);
    char error[256];
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        (void)fprintf(stderr, "full-ledger slaying validation failed: %s\n",
                      error);
        CC_CHECK(false);
    }
}

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xd12a600b));
    char error[256];

    CcSim tunnel;
    CcSimInit(&tunnel, UINT32_C(0x6f626c69));
    CcId town_id = tunnel.player.location_id;
    CcCommand climb = {
        .kind = CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL,
        .target_id = tunnel.dragon.lair_settlement_id
    };
    CC_CHECK(!CcSimApply(&tunnel, &climb, error, sizeof(error)));
    CC_CHECK(tunnel.player.location_id == town_id);
    CC_CHECK(tunnel.carriage.location_id == town_id);

    tunnel.player.location_id = tunnel.goblins.lair_settlement_id;
    tunnel.carriage.location_id = tunnel.goblins.lair_settlement_id;
    int32_t tunnel_start_day = tunnel.current_day;
    CC_CHECK(CcSimApply(&tunnel, &climb, error, sizeof(error)));
    CC_CHECK(tunnel.current_day == tunnel_start_day + 1);
    CC_CHECK(tunnel.player.location_id == tunnel.dragon.lair_settlement_id);
    CC_CHECK(tunnel.carriage.location_id == tunnel.goblins.lair_settlement_id);
    CC_CHECK(CcSimValidate(&tunnel, error, sizeof(error)));
    CC_CHECK(CountEvents(&tunnel, CC_EVENT_GOBLIN_TUNNEL_TRAVERSED) == 1);

    CcCommand descend = {
        .kind = CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL,
        .target_id = tunnel.goblins.lair_settlement_id
    };
    CC_CHECK(CcSimApply(&tunnel, &descend, error, sizeof(error)));
    CC_CHECK(tunnel.current_day == tunnel_start_day + 2);
    CC_CHECK(tunnel.player.location_id == tunnel.goblins.lair_settlement_id);
    CC_CHECK(tunnel.carriage.location_id == tunnel.goblins.lair_settlement_id);
    CC_CHECK(CountEvents(&tunnel, CC_EVENT_GOBLIN_TUNNEL_TRAVERSED) == 2);
    if (!CcSimValidate(&tunnel, error, sizeof(error))) {
        (void)fprintf(stderr, "%s:%d: tunnel validation failed: %s\n",
                      __FILE__, __LINE__, error);
        return 1;
    }

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

    CcSim intercepted;
    CcSimInit(&intercepted, UINT32_C(0x1a7e2ce7));
    intercepted.player.location_id = intercepted.dragon.lair_settlement_id;
    intercepted.carriage.location_id = intercepted.player.location_id;
    intercepted.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
    intercepted.goblins.raid_motive = CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    intercepted.goblins.tribute_target_id =
        intercepted.dragon.lair_settlement_id;
    intercepted.goblins.tribute_days_remaining = 3;
    const CcEvent *tribute_parent = CcSimRecentEvent(&intercepted, 0);
    CC_CHECK(tribute_parent != NULL);
    intercepted.goblins.tribute_event_id = tribute_parent->id;
    intercepted.goblins.carried_tribute = 12;
    intercepted.goblins.carried_goods[CC_GOOD_GOLD] = 1;
    intercepted.goblins.carried_goods[CC_GOOD_GEMS] = 1;
    CcTreasure *tribute_relic =
        &intercepted.treasures[intercepted.treasure_count++];
    *tribute_relic = (CcTreasure){
        .id = CcMakeId(CC_ENTITY_TREASURE, UINT64_C(9102)),
        .maker_settlement_id = intercepted.settlements[0].id,
        .owner_id = intercepted.goblins.id,
        .location_id = intercepted.goblins.lair_settlement_id,
        .gold_content = 1,
        .gem_content = 1,
        .craft_work = 2,
        .appraised_value = 90,
        .created_day = 1
    };
    (void)snprintf(tribute_relic->name, sizeof(tribute_relic->name),
                   "The Ember Tithe");
    intercepted.next_entity_serial = UINT64_C(9103);
    intercepted.goblins.carried_treasure_id = tribute_relic->id;
    CcMoney intercepted_hoard = intercepted.dragon.hoard;
    CcMoney intercepted_coins = intercepted.player.coins;
    CcCommand intercept = {
        .kind = CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE
    };
    CC_CHECK(CcSimApply(&intercepted, &intercept,
                        error, sizeof(error)));
    CC_CHECK(intercepted.player.coins == intercepted_coins + 12);
    CC_CHECK(intercepted.player.cargo[CC_GOOD_GOLD] == 1);
    CC_CHECK(intercepted.player.cargo[CC_GOOD_GEMS] == 1);
    CC_CHECK(intercepted.player.treasure_cargo_slots == 1);
    CC_CHECK(tribute_relic->owner_id == intercepted.player.id);
    CC_CHECK(tribute_relic->location_id == intercepted.player.location_id);
    CC_CHECK(intercepted.dragon.hoard == intercepted_hoard);
    CC_CHECK(intercepted.dragon.stolen_outstanding == 0);
    CC_CHECK(intercepted.dragon.omen_days_remaining == 0);
    CC_CHECK(intercepted.goblins.tribute_phase ==
             CC_GOBLIN_TRIBUTE_IDLE);
    CC_CHECK(CountEvents(
        &intercepted, CC_EVENT_GOBLIN_TRIBUTE_TAKEN) == 1);
    CC_CHECK(CcSimValidate(&intercepted, error, sizeof(error)));

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
    const CcEvent *social_guard = social_theft != NULL ?
        CcSimEvent(&unjust, social_theft->parent_id) : NULL;
    const CcEvent *social_departure = social_guard != NULL ?
        CcSimEvent(&unjust, social_guard->parent_id) : NULL;
    const CcEvent *inequality = social_departure != NULL ?
        CcSimEvent(&unjust, social_departure->parent_id) : NULL;
    CC_CHECK(social_theft != NULL &&
             social_theft->subject_id == unjust.hoard_raiders.id);
    CC_CHECK(social_guard != NULL &&
             social_guard->kind == CC_EVENT_GOBLIN_HOARD_DEFENDED);
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
    CC_CHECK(unjust.hoard_raiders.social_raid_latched);
    CC_CHECK(unjust.goblins.hoard_defenses == 1);
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
    war.diplomacy[1][2] = CC_DIPLOMACY_WAR;
    war.diplomacy[2][1] = CC_DIPLOMACY_WAR;
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
    war.royal_carriages[1].location_id = war_supplier->id;
    fortress->stock[CC_GOOD_FOOD] = 0;
    fortress->stock[CC_GOOD_WHEAT] = 0;
    fortress->stock[CC_GOOD_MEAT] = 0;
    fortress->stock[CC_GOOD_TOOLS] = 0;
    fortress->reserve_target[CC_GOOD_FOOD] = 40;
    fortress->reserve_target[CC_GOOD_TOOLS] = 4;
    fortress->production[CC_GOOD_FOOD] = 0;
    fortress->consumption[CC_GOOD_FOOD] = 6;
    war_supplier->consumption[CC_GOOD_FOOD] = 10;
    war_supplier->stock[CC_GOOD_TOOLS] = 0;
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
    const CcEvent *war_guard = war_theft != NULL ?
        CcSimEvent(&war, war_theft->parent_id) : NULL;
    const CcEvent *war_departure = war_guard != NULL ?
        CcSimEvent(&war, war_guard->parent_id) : NULL;
    const CcEvent *war_pressure = war_departure != NULL ?
        CcSimEvent(&war, war_departure->parent_id) : NULL;
    CC_CHECK(war_theft != NULL &&
             war_theft->subject_id == war.hoard_raiders.id);
    CC_CHECK(war_guard != NULL &&
             war_guard->kind == CC_EVENT_GOBLIN_HOARD_DEFENDED);
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
    CC_CHECK(war.hoard_raiders.war_raid_latched);
    CC_CHECK(war.goblins.hoard_defenses == 1);
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
    CC_CHECK(war_restored.hoard_raiders.war_raid_latched);
    CC_CHECK(war_restored.goblins.hoard_defenses == 1);
    CC_CHECK(remove(war_path) == 0);

    CcSim dragon_host;
    CcSimInit(&dragon_host, UINT32_C(0xd2a60a11));
    for (int32_t first = 0; first < dragon_host.kingdom_count; ++first) {
        dragon_host.dragon.hoard += dragon_host.kingdoms[first].treasury;
        dragon_host.kingdoms[first].treasury = 0;
        for (int32_t second = 0;
             second < dragon_host.kingdom_count; ++second) {
            if (first == second) continue;
            dragon_host.diplomacy[first][second] = CC_DIPLOMACY_ALLIANCE;
        }
    }
    dragon_host.dragon_campaign.pledged_kingdom_mask = UINT32_C(7);
    dragon_host.goblins.members = 12;
    dragon_host.goblins.devotion = 0;
    dragon_host.goblins.hoard_defenses = 0;
    dragon_host.dragon.body_condition = 10;
    dragon_host.dragon.crown_strength = 0;
    dragon_host.dragon.territory_stability = 0;
    dragon_host.dragon.memory_integrity = 0;
    dragon_host.dragon.age_days = 500 * 365;
    for (int32_t i = 0; i < dragon_host.settlement_count; ++i) {
        dragon_host.settlements[i].stock[CC_GOOD_FOOD] += 32;
        dragon_host.settlements[i].stock[CC_GOOD_TOOLS] += 8;
        dragon_host.settlements[i].stock[CC_GOOD_WEAPONS] += 12;
    }
    CcMoney campaign_gold = CcSimTrackedGold(&dragon_host);
    CcSimAdvanceDays(&dragon_host, 55);
    CC_CHECK(dragon_host.dragon.slain);
    CC_CHECK(dragon_host.dragon.hoard == 0);
    CC_CHECK(dragon_host.dragon_campaign.phase ==
             CC_DRAGON_CAMPAIGN_IDLE);
    CC_CHECK(dragon_host.dragon_campaign.attempts == 1);
    CC_CHECK(dragon_host.dragon_campaign.victories == 1);
    CC_CHECK(dragon_host.dragon_campaign.defeats == 0);
    CC_CHECK(CountEvents(&dragon_host, CC_EVENT_DRAGON_BATTLE) == 0);
    CC_CHECK(CountEvents(&dragon_host, CC_EVENT_DRAGON_SLAIN) == 1);
    CC_CHECK(CountEvents(&dragon_host,
                         CC_EVENT_DRAGON_HOARD_RECOVERED) == 1);
    CC_CHECK(CcSimTrackedGold(&dragon_host) == campaign_gold);
    CC_CHECK(CcSimValidate(&dragon_host, error, sizeof(error)));

    CcSim learning_host;
    CcSimInit(&learning_host, UINT32_C(0xd2a61ea7));
    learning_host.dragon_campaign.phase = CC_DRAGON_CAMPAIGN_OUTBOUND;
    learning_host.dragon_campaign.alliance_kingdom_mask = UINT32_C(7);
    learning_host.dragon_campaign.origin_settlement_id =
        learning_host.settlements[0].id;
    learning_host.dragon_campaign.days_remaining = 1;
    learning_host.dragon_campaign.supplies[CC_GOOD_FOOD] = 32;
    learning_host.dragon_campaign.supplies[CC_GOOD_TOOLS] = 8;
    learning_host.dragon_campaign.supplies[CC_GOOD_WEAPONS] = 12;
    learning_host.goblins.members = 100;
    learning_host.goblins.devotion = 100;
    learning_host.goblins.hoard_defenses = 12;
    learning_host.dragon.body_condition = 90;
    CcSimAdvanceDays(&learning_host, 1);
    CC_CHECK(learning_host.dragon_campaign.defeats == 1);
    CC_CHECK(CcDragonCampaignExperience(&learning_host) == 12);
    CC_CHECK(learning_host.dragon.body_condition < 90);
    CC_CHECK(learning_host.dragon.memory_integrity < 100);
    learning_host.dragon_campaign.defeats = 20;
    CC_CHECK(CcDragonCampaignExperience(&learning_host) == 72);

    CcSim alliance_peace;
    CcSimInit(&alliance_peace, UINT32_C(0xa111a9ce));
    alliance_peace.dragon.slain = true;
    alliance_peace.dragon.slain_day = alliance_peace.current_day;
    alliance_peace.dragon.stolen_outstanding = 0;
    alliance_peace.dragon.theft_actor_id = 0U;
    alliance_peace.dragon.retaliation_target_id = 0U;
    alliance_peace.dragon.omen_event_id = 0U;
    alliance_peace.dragon.omen_days_remaining = 0;
    alliance_peace.diplomacy[0][1] = CC_DIPLOMACY_ALLIANCE;
    alliance_peace.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    alliance_peace.routes[1].closed = false;
    alliance_peace.routes[1].security = 100;
    alliance_peace.routes[1].condition = 100;
    alliance_peace.settlements[2].security = 100;
    alliance_peace.kingdoms[1].legitimacy = 100;
    alliance_peace.bandit_count = 0;
    alliance_peace.monster_count = 0;
    alliance_peace.courier_count = 1;
    alliance_peace.couriers[0] = (CcCourier){
        .id = CcMakeId(CC_ENTITY_COURIER, UINT64_C(9999)),
        .kind = CC_COURIER_PEACE_OFFER,
        .status = CC_COURIER_TRAVELLING,
        .issuer_kingdom_id = alliance_peace.kingdoms[0].id,
        .recipient_kingdom_id = alliance_peace.kingdoms[1].id,
        .origin_settlement_id = alliance_peace.settlements[1].id,
        .destination_settlement_id = alliance_peace.settlements[2].id,
        .current_settlement_id = alliance_peace.settlements[1].id,
        .route_id = alliance_peace.routes[1].id,
        .departure_day = alliance_peace.current_day,
        .arrival_day = alliance_peace.current_day + 1,
        .reliability = 100
    };
    alliance_peace.next_entity_serial = UINT64_C(10000);
    CcSimAdvanceDays(&alliance_peace, 1);
    CC_CHECK(alliance_peace.couriers[0].status == CC_COURIER_DELIVERED);
    CC_CHECK(alliance_peace.diplomacy[0][1] == CC_DIPLOMACY_PEACE &&
             alliance_peace.diplomacy[1][0] == CC_DIPLOMACY_PEACE);
    CC_CHECK(CountEvents(&alliance_peace, CC_EVENT_PEACE_DECLARED) == 1);

    CcSim empty_court;
    CcSimInit(&empty_court, UINT32_C(0xe607c017));
    CcId empty_kingdom = empty_court.kingdoms[1].id;
    for (int32_t place = 0; place < empty_court.settlement_count; ++place) {
        CcSettlement *settlement = &empty_court.settlements[place];
        if (settlement->kingdom_id != empty_kingdom) continue;
        settlement->population = 0;
        settlement->security = 0;
        settlement->prosperity = 0;
        settlement->service_mask = 0U;
        settlement->service_project = CC_SERVICE_NONE;
        settlement->service_project_days = 0;
    }
    empty_court.dragon.regional_influence = 100;
    empty_court.courier_count = 0;
    CcSimAdvanceDays(&empty_court, 27);
    CC_CHECK(empty_court.courier_count == 0);
    if (!CcSimValidate(&empty_court, error, sizeof(error))) {
        (void)fprintf(stderr, "%s:%d: empty court validation failed: %s\n",
                      __FILE__, __LINE__, error);
        return 1;
    }

    CcSim territorial_peace;
    CcSimInit(&territorial_peace, UINT32_C(0x7e221701));
    territorial_peace.dragon.slain = true;
    territorial_peace.dragon.slain_day = territorial_peace.current_day;
    territorial_peace.routes[1].closed = false;
    territorial_peace.routes[1].security = 100;
    territorial_peace.routes[1].condition = 100;
    territorial_peace.settlements[2].security = 100;
    territorial_peace.kingdoms[0].legitimacy = 100;
    territorial_peace.kingdoms[0].treasury = 5000;
    territorial_peace.kingdoms[1].legitimacy = 100;
    territorial_peace.kingdoms[1].treasury = 0;
    for (int32_t place = 0;
         place < territorial_peace.settlement_count; ++place) {
        CcSettlement *settlement = &territorial_peace.settlements[place];
        if (settlement->kingdom_id == territorial_peace.kingdoms[0].id) {
            settlement->security = 100;
            settlement->prosperity = 100;
            settlement->hunger = 0;
        } else if (settlement->kingdom_id ==
                   territorial_peace.kingdoms[1].id) {
            settlement->security = 10;
            settlement->prosperity = 10;
            settlement->hunger = 60;
        }
    }
    territorial_peace.settlements[2].security = 100;
    territorial_peace.courier_count = 1;
    territorial_peace.couriers[0] = (CcCourier){
        .id = CcMakeId(CC_ENTITY_COURIER, UINT64_C(10001)),
        .kind = CC_COURIER_PEACE_OFFER,
        .status = CC_COURIER_TRAVELLING,
        .issuer_kingdom_id = territorial_peace.kingdoms[0].id,
        .recipient_kingdom_id = territorial_peace.kingdoms[1].id,
        .origin_settlement_id = territorial_peace.settlements[1].id,
        .destination_settlement_id = territorial_peace.settlements[2].id,
        .current_settlement_id = territorial_peace.settlements[1].id,
        .route_id = territorial_peace.routes[1].id,
        .departure_day = territorial_peace.current_day,
        .arrival_day = territorial_peace.current_day + 1,
        .reliability = 100
    };
    CcSimAdvanceDays(&territorial_peace, 1);
    int32_t winner_settlements = 0;
    int32_t loser_settlements = 0;
    for (int32_t place = 0;
         place < territorial_peace.settlement_count; ++place) {
        if (territorial_peace.settlements[place].kingdom_id ==
            territorial_peace.kingdoms[0].id) winner_settlements += 1;
        if (territorial_peace.settlements[place].kingdom_id ==
            territorial_peace.kingdoms[1].id) loser_settlements += 1;
    }
    CC_CHECK(winner_settlements == 3);
    CC_CHECK(loser_settlements == 1);
    CC_CHECK(territorial_peace.diplomacy[0][1] ==
             CC_DIPLOMACY_PEACE);

    CcSim dormant_alliance;
    CcSimInit(&dormant_alliance, UINT32_C(0xa111d0a0));
    dormant_alliance.current_day = 20 * 112 - 1;
    dormant_alliance.dragon.hoard = 0;
    dormant_alliance.diplomacy[0][1] = CC_DIPLOMACY_ALLIANCE;
    dormant_alliance.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    dormant_alliance.diplomacy_changed_day[0][1] =
        dormant_alliance.current_day - 4 * 364;
    dormant_alliance.diplomacy_changed_day[1][0] =
        dormant_alliance.diplomacy_changed_day[0][1];
    dormant_alliance.courier_count = 0;
    CcSimAdvanceDays(&dormant_alliance, 1);
    bool peace_sent = false;
    for (int32_t courier = 0;
         courier < dormant_alliance.courier_count; ++courier) {
        if (dormant_alliance.couriers[courier].kind ==
            CC_COURIER_PEACE_OFFER) peace_sent = true;
    }
    CC_CHECK(peace_sent);
    CcSimAdvanceDays(&dormant_alliance, 30);
    CC_CHECK(dormant_alliance.diplomacy[0][1] ==
             CC_DIPLOMACY_PEACE);

    CcSim crisis_alliance;
    CcSimInit(&crisis_alliance, UINT32_C(0xa111c215));
    crisis_alliance.current_day = 30 * 112 - 1;
    crisis_alliance.dragon.hoard =
        CcSimTrackedGold(&crisis_alliance) * 2;
    crisis_alliance.dragon.age_days = 500 * 365;
    crisis_alliance.dragon_campaign.defeats = 1;
    crisis_alliance.diplomacy[0][1] = CC_DIPLOMACY_ALLIANCE;
    crisis_alliance.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    crisis_alliance.diplomacy_changed_day[0][1] =
        crisis_alliance.current_day - 8 * 364;
    crisis_alliance.diplomacy_changed_day[1][0] =
        crisis_alliance.diplomacy_changed_day[0][1];
    crisis_alliance.courier_count = 0;
    CcSimAdvanceDays(&crisis_alliance, 1);
    bool feud_sent = false;
    for (int32_t courier = 0;
         courier < crisis_alliance.courier_count; ++courier) {
        if (crisis_alliance.couriers[courier].kind ==
            CC_COURIER_WAR_DECLARATION) feud_sent = true;
    }
    CC_CHECK(feud_sent);

    CheckFullLedgerDragonTheft();
    CheckFullLedgerDragonSlaying();

    puts("Goblin tribute, war finance, and dragon retaliation tests passed");
    return 0;
}
