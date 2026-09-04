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

static int32_t TotalHuntFood(const CcSim *sim)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].stock[CC_GOOD_FOOD];
        total += sim->settlements[i].cow_adults * 3;
        total += sim->settlements[i].sheep_adults;
    }
    return total;
}

int main(void)
{
    char error[256];
    CcSim offices;
    CcSimInit(&offices, UINT32_C(0xab807001));
    CC_CHECK(CcSimCharacter(
        &offices, offices.archives.abbot_character_id) != NULL);
    for (int32_t i = 0; i < offices.kingdom_count; ++i) {
        CC_CHECK(CcSimCharacter(
            &offices, offices.kingdoms[i].ruler_character_id) != NULL);
        CC_CHECK(CcSimCharacter(
            &offices, offices.kingdoms[i].monastery_patron_id) != NULL);
        CC_CHECK(offices.kingdoms[i].sanction >= 35);
    }
    CcSimAdvanceDays(&offices, 7);
    int32_t anointed_realms = 0;
    for (int32_t i = 0; i < offices.kingdom_count; ++i) {
        if (offices.kingdoms[i].anointed_by_character_id != 0U) {
            anointed_realms += 1;
            CC_CHECK(offices.kingdoms[i].anointed_by_character_id ==
                     offices.archives.abbot_character_id);
        }
    }
    CC_CHECK(anointed_realms == 1);
    CC_CHECK(CountEvents(&offices, CC_EVENT_KING_ANOINTED) == 1);

    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xd2a60ec0));
    CC_CHECK(sim.dragon.life_stage == CC_DRAGON_STAGE_CROWNED);
    CC_CHECK(sim.dragon.activity == CC_DRAGON_ACTIVITY_DORMANT);
    CC_CHECK(sim.dragon.crown_strength >= 20);
    CC_CHECK(sim.dragon.body_condition == 82);
    CC_CHECK(sim.dragon.memory_integrity == 100);

    int32_t crown_before = sim.dragon.crown_strength;
    sim.dragon.hoard = 500;
    sim.dragon.hoard_goods[CC_GOOD_GOLD] = 4;
    sim.dragon.hoard_goods[CC_GOOD_GEMS] = 2;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.crown_strength > crown_before);

    int32_t food_before = TotalHuntFood(&sim);
    sim.dragon.body_condition = 20;
    sim.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.hunts == 1);
    CC_CHECK(TotalHuntFood(&sim) < food_before);
    CC_CHECK(sim.dragon.retaliations == 0);
    CC_CHECK(CountEvents(&sim, CC_EVENT_DRAGON_HUNT) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_DRAGON_RETALIATION) == 0);

    sim.player.location_id = sim.dragon.lair_settlement_id;
    sim.carriage.location_id = sim.player.location_id;
    int32_t memory_before = sim.dragon.memory_integrity;
    int32_t continuity_before = sim.dragon.crown_continuity_days;
    CcCommand steal = {
        .kind = CC_COMMAND_STEAL_DRAGON_HOARD,
        .amount = 20
    };
    CC_CHECK(CcSimApply(&sim, &steal, error, sizeof(error)));
    CC_CHECK(sim.dragon.memory_integrity < memory_before);
    CC_CHECK(sim.dragon.crown_continuity_days ==
             continuity_before - 182);
    CC_CHECK(sim.dragon.activity == CC_DRAGON_ACTIVITY_RETALIATING);
    CcCommand restore = {
        .kind = CC_COMMAND_RETURN_DRAGON_TREASURE,
        .amount = 20
    };
    CC_CHECK(CcSimApply(&sim, &restore, error, sizeof(error)));
    CC_CHECK(sim.dragon.memory_integrity >= 75);
    CC_CHECK(sim.dragon.stolen_outstanding == 0);

    CcTreasure *remembered = &sim.treasures[sim.treasure_count++];
    *remembered = (CcTreasure){
        .id = CcMakeId(CC_ENTITY_TREASURE, UINT64_C(9001)),
        .maker_settlement_id = sim.settlements[0].id,
        .owner_id = sim.dragon.id,
        .location_id = sim.dragon.lair_settlement_id,
        .gold_content = 2,
        .gem_content = 2,
        .craft_work = 3,
        .appraised_value = 240,
        .created_day = 1
    };
    (void)snprintf(remembered->name, sizeof(remembered->name),
                   "The First Crown's Seal");
    sim.next_entity_serial = UINT64_C(9002);
    memory_before = sim.dragon.memory_integrity;
    CcCommand steal_named = {
        .kind = CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE,
        .target_id = remembered->id
    };
    CC_CHECK(CcSimApply(&sim, &steal_named, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_treasure_id == remembered->id);
    CC_CHECK(sim.dragon.stolen_outstanding == remembered->appraised_value);
    CC_CHECK(sim.player.treasure_cargo_slots == 1);
    CC_CHECK(sim.dragon.memory_integrity < memory_before);
    CC_CHECK(!CcSimApply(&sim, &restore, error, sizeof(error)));
    CcCommand return_named = {
        .kind = CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE,
        .target_id = remembered->id
    };
    CC_CHECK(CcSimApply(&sim, &return_named, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_treasure_id == 0U);
    CC_CHECK(sim.dragon.stolen_outstanding == 0);
    CC_CHECK(sim.player.treasure_cargo_slots == 0);
    CC_CHECK(sim.dragon.memory_integrity >= 90);
    CC_CHECK(remembered->owner_id == sim.dragon.id);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

    CcSim deep_wyrm;
    CcSimInit(&deep_wyrm, UINT32_C(0xdee00001));
    deep_wyrm.dragon.hoard = 5000;
    deep_wyrm.dragon.hoard_goods[CC_GOOD_GOLD] = 10;
    deep_wyrm.dragon.hoard_goods[CC_GOOD_GEMS] = 10;
    deep_wyrm.dragon.age_days = 500 * 365 - 1;
    deep_wyrm.dragon.body_condition = 50;
    deep_wyrm.dragon.memory_integrity = 100;
    deep_wyrm.dragon.territory_stability = 100;
    deep_wyrm.dragon.crown_continuity_days = 250 * 365;
    deep_wyrm.goblins.devotion = 100;
    CcSimAdvanceDays(&deep_wyrm, 1);
    CC_CHECK(deep_wyrm.dragon.crown_strength >= 60);
    CC_CHECK(deep_wyrm.dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM);

    /* Relic: Deep Wyrm ascension leaves the Wyrmheart in the lair. */
    CcSim wyrm;
    CcSimInit(&wyrm, UINT32_C(0xdee00002));
    wyrm.dragon.hoard = 5000;
    wyrm.dragon.hoard_goods[CC_GOOD_GOLD] = 10;
    wyrm.dragon.hoard_goods[CC_GOOD_GEMS] = 10;
    wyrm.dragon.age_days = 500 * 365 - 1;
    wyrm.dragon.body_condition = 50;
    wyrm.dragon.memory_integrity = 100;
    wyrm.dragon.territory_stability = 100;
    wyrm.dragon.crown_continuity_days = 250 * 365;
    wyrm.dragon.crown_strength = 60;
    wyrm.goblins.devotion = 100;
    CcSimAdvanceDays(&wyrm, 1);
    CC_CHECK(wyrm.dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM);
    bool wyrmheart_found = false;
    for (int32_t i = 0; i < wyrm.treasure_count; ++i) {
        if (strncmp(wyrm.treasures[i].name, "Wyrmheart", 9) == 0 &&
            !wyrm.treasures[i].destroyed) {
            wyrmheart_found = true;
            CC_CHECK(wyrm.treasures[i].owner_id == wyrm.dragon.id);
            CC_CHECK(wyrm.treasures[i].gem_content == 12);
        }
    }
    CC_CHECK(wyrmheart_found);

    CcSim brood;
    CcSimInit(&brood, UINT32_C(0xb200dc0d));
    brood.dragon.hoard = 5000;
    brood.dragon.hoard_goods[CC_GOOD_GOLD] = 10;
    brood.dragon.hoard_goods[CC_GOOD_GEMS] = 10;
    brood.dragon.body_condition = 100;
    brood.dragon.memory_integrity = 100;
    brood.dragon.territory_stability = 100;
    brood.dragon.crown_continuity_days = 250 * 365;
    brood.dragon.brood_cooldown_days = 1;
    brood.goblins.devotion = 100;
    brood.goblins.lair_stock[CC_GOOD_FOOD] = 100;
    CcSimAdvanceDays(&brood, 1);
    CC_CHECK(brood.dragon.egg_count >= 1);
    CC_CHECK(brood.dragon.egg_count <= 3);
    CC_CHECK(brood.dragon.brood_days_remaining >= 7 * 365);
    CC_CHECK(brood.dragon.activity == CC_DRAGON_ACTIVITY_BROODING);
    CC_CHECK(brood.dragon.broods_laid == 1);
    CC_CHECK(CountEvents(&brood, CC_EVENT_DRAGON_BROOD) == 1);

    CcSim dispossessed;
    CcSimInit(&dispossessed, UINT32_C(0xd155055e));
    dispossessed.dragon.territory_stability = 0;
    dispossessed.dragon.territoryless_days = 0;
    CcSimAdvanceDays(&dispossessed, 1);
    CC_CHECK(dispossessed.dragon.territoryless_days == 1);
    CC_CHECK(CountEvents(
        &dispossessed, CC_EVENT_DRAGON_TERRITORY_LOST) == 1);
    dispossessed.dragon.life_stage = CC_DRAGON_STAGE_CROWNED;
    dispossessed.dragon.territoryless_days = 5 * 364 - 1;
    CcSimAdvanceDays(&dispossessed, 1);
    CC_CHECK(dispossessed.dragon.life_stage ==
             CC_DRAGON_STAGE_UNCROWNED);
    CC_CHECK(CountEvents(
        &dispossessed, CC_EVENT_DRAGON_UNCROWNED) == 1);

    CcSim stockpile;
    CcSimInit(&stockpile, UINT32_C(0x570c901e));
    stockpile.dragon.age_days = 500 * 365;
    stockpile.dragon.regional_influence = 80;
    stockpile.dragon_campaign.pledged_kingdom_mask = UINT32_C(7);
    for (int32_t i = 0; i < stockpile.settlement_count; ++i) {
        if (stockpile.settlements[i].kingdom_id ==
            stockpile.kingdoms[0].id) {
            stockpile.settlements[i].population = 0;
        }
        stockpile.settlements[i].stock[CC_GOOD_FOOD] += 100;
        stockpile.settlements[i].stock[CC_GOOD_IRON] = 0;
        stockpile.settlements[i].stock[CC_GOOD_WOOD] += 100;
        stockpile.settlements[i].stock[CC_GOOD_STONE] = 0;
        stockpile.settlements[i].stock[CC_GOOD_TOOLS] = 0;
        stockpile.settlements[i].stock[CC_GOOD_WEAPONS] = 0;
    }
    bool patron_named = false;
    for (int32_t day = 0;
         day < 220 && stockpile.dragon_campaign.attempts == 0; ++day) {
        CcSimAdvanceDays(&stockpile, 1);
        if (CountEvents(
                &stockpile, CC_EVENT_DRAGON_PATRON_NAMED) > 0) {
            patron_named = true;
        }
    }
    CC_CHECK(stockpile.dragon_campaign.attempts >= 1);
    CC_CHECK(CcSimCharacter(
        &stockpile,
        stockpile.dragon_campaign.patron_character_id) != NULL);
    CC_CHECK(CcSimCharacter(
        &stockpile, stockpile.dragon_campaign.hero_character_id) != NULL);
    CC_CHECK(patron_named);

    /* Relic: slaying the dragon in a campaign forges the Bane blade with
       provenance chained to the slaying event and held by the champion. */
    CcSim bane;
    CcSimInit(&bane, UINT32_C(0xba4e0001));
    CcSettlement *origin = CcSimSettlementMutable(&bane,
        bane.dragon_campaign.origin_settlement_id);
    if (origin == NULL) origin = &bane.settlements[0];
    bane.dragon_campaign.origin_settlement_id = origin->id;
    bane.dragon_campaign.phase = CC_DRAGON_CAMPAIGN_OUTBOUND;
    bane.dragon_campaign.days_remaining = 0;
    bane.dragon_campaign.pledged_kingdom_mask = 1U;
    bane.dragon_campaign.alliance_kingdom_mask = 7U;
    bane.dragon_campaign.cause_event_id = 0U;
    bane.dragon_campaign.patron_character_id =
        bane.kingdoms[0].monastery_patron_id;
    bane.dragon_campaign.hero_character_id = bane.characters[1].id;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        bane.dragon_campaign.supplies[good] = 400;
    }
    CcSimAdvanceDays(&bane, 1);
    CC_CHECK(bane.dragon.slain);
    bool bane_found = false;
    int32_t bane_slot = -1;
    for (int32_t i = 0; i < bane.treasure_count; ++i) {
        const CcTreasure *t = &bane.treasures[i];
        if (strncmp(t->name, "Bane of ", 8) == 0 && !t->destroyed) {
            bane_found = true;
            bane_slot = i;
            CC_CHECK(t->gold_content == 3);
            CC_CHECK(t->owner_id ==
                     bane.dragon_campaign.hero_character_id);
        }
    }
    CC_CHECK(bane_found);
    CcId first_bearer = bane.treasures[bane_slot].owner_id;
    CcCharacter *bearer = (CcCharacter *)CcSimCharacter(
        &bane, first_bearer);
    CC_CHECK(bearer != NULL);
    bearer->death_day = bane.current_day;
    CcSimAdvanceDays(&bane, 1);
    const CcCharacter *heir = CcSimCharacter(
        &bane, bane.treasures[bane_slot].owner_id);
    CC_CHECK(heir != NULL);
    CC_CHECK(heir->ancestor_id == first_bearer);

    const char *path = "/tmp/crownless-dragon-ecology-tests.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &brood, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&brood));
    CC_CHECK(restored.dragon.egg_count == brood.dragon.egg_count);
    CC_CHECK(restored.dragon.brood_days_remaining ==
             brood.dragon.brood_days_remaining);
    CC_CHECK(remove(path) == 0);

    restored.dragon.slain = true;
    restored.dragon.slain_day = restored.current_day;
    restored.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    restored.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    restored.dragon.body_condition = 0;
    restored.dragon.crown_strength = 0;
    restored.dragon.hoard = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        restored.dragon.hoard_goods[good] = 0;
    }
    restored.dragon.egg_count = 2;
    restored.dragon.brood_days_remaining = 1;
    CcId dead_dragon_id = restored.dragon.id;
    CcTreasure *inherited = &restored.treasures[restored.treasure_count++];
    *inherited = (CcTreasure){
        .id = CcMakeId(
            CC_ENTITY_TREASURE, restored.next_entity_serial++),
        .maker_settlement_id = restored.dragon.lair_settlement_id,
        .owner_id = dead_dragon_id,
        .location_id = restored.dragon.lair_settlement_id,
        .gold_content = 1,
        .gem_content = 1,
        .craft_work = 1,
        .appraised_value = 90,
        .created_day = 1
    };
    (void)snprintf(inherited->name, sizeof(inherited->name),
                   "The Inherited Scale");
    CcSimAdvanceDays(&restored, 1);
    CC_CHECK(!restored.dragon.slain);
    CC_CHECK(restored.dragon.id != dead_dragon_id);
    CC_CHECK(restored.dragon.life_stage == CC_DRAGON_STAGE_WHELP);
    CC_CHECK(restored.treasures[0].owner_id == restored.dragon.id);
    CC_CHECK(restored.dragon.egg_count == 0);
    CC_CHECK(CountEvents(&restored, CC_EVENT_DRAGON_SUCCESSOR) == 1);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));

    restored.dragon.age_days = 15 * 365 - 1;
    CcSimAdvanceDays(&restored, 1);
    CC_CHECK(restored.dragon.life_stage == CC_DRAGON_STAGE_WANDERER);

    CcSim cult;
    CcSimInit(&cult, UINT32_C(0xc0175eed));
    cult.dragon.slain = true;
    cult.dragon.slain_day = 1;
    cult.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    cult.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    cult.dragon.body_condition = 0;
    cult.dragon.crown_strength = 0;
    cult.dragon.stolen_outstanding = 0;
    cult.dragon.stolen_treasure_id = 0U;
    cult.dragon.theft_actor_id = 0U;
    cult.dragon.retaliation_target_id = 0U;
    cult.dragon.omen_event_id = 0U;
    cult.dragon.omen_days_remaining = 0;
    cult.dragon.egg_count = 0;
    cult.dragon.brood_days_remaining = 0;
    cult.dragon.afterdeath_days = 364;
    cult.goblins.members = 30;
    cult.goblins.devotion = 50;
    cult.goblins.lair_stock[CC_GOOD_FOOD] = 32;
    cult.goblins.lair_stock[CC_GOOD_TOOLS] = 3;
    cult.goblins.lair_stock[CC_GOOD_WEAPONS] = 4;
    cult.goblins.tribute_cooldown_days = 1000;
    int32_t cult_members = cult.goblins.members;
    CcSimAdvanceDays(&cult, 1);
    CC_CHECK(cult.goblins.members > cult_members);
    CC_CHECK(cult.goblins.devotion > 50);
    CC_CHECK(CountEvents(&cult, CC_EVENT_GOBLIN_CULT_RALLIED) == 1);

    CcSim offering_recovery;
    CcSimInit(&offering_recovery, UINT32_C(0x0ffee110));
    offering_recovery.dragon.slain = true;
    offering_recovery.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    offering_recovery.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    offering_recovery.dragon.afterdeath_days = 100 * 365 - 1;
    offering_recovery.current_day = 100 * 365 - 1;
    offering_recovery.settlements[0].market_coins = 1000;
    for (int32_t i = 0; i < offering_recovery.settlement_count; ++i) {
        offering_recovery.settlements[i].stock[CC_GOOD_GOLD] = 0;
        offering_recovery.settlements[i].stock[CC_GOOD_GEMS] = 0;
    }
    offering_recovery.settlements[1].stock[CC_GOOD_GOLD] = 2;
    offering_recovery.goblins.lair_stock[CC_GOOD_GOLD] = 0;
    offering_recovery.goblins.lair_stock[CC_GOOD_GEMS] = 0;
    offering_recovery.goblins.lair_stock[CC_GOOD_IRON] = 0;
    offering_recovery.goblins.lair_stock[CC_GOOD_TOOLS] = 0;
    offering_recovery.goblins.lair_stock[CC_GOOD_WEAPONS] = 0;
    offering_recovery.goblins.tribute_cooldown_days = 1000;
    CcSimAdvanceDays(&offering_recovery, 1);
    CC_CHECK(offering_recovery.goblins.lair_stock[CC_GOOD_GOLD] == 1);
    CC_CHECK(offering_recovery.goblins.lair_stock[CC_GOOD_TOOLS] == 2);
    CC_CHECK(offering_recovery.goblins.lair_stock[CC_GOOD_WEAPONS] == 3);

    cult.current_day = 120 * 365;
    cult.dragon.afterdeath_days = 120 * 365 - 1;
    cult.goblins.members = 84;
    cult.goblins.devotion = 90;
    cult.goblins.cohesion = 90;
    cult.goblins.lair_coins = 120;
    cult.goblins.lair_stock[CC_GOOD_FOOD] = 64;
    cult.goblins.lair_stock[CC_GOOD_TOOLS] = 4;
    cult.goblins.lair_stock[CC_GOOD_WEAPONS] = 4;
    cult.goblins.lair_stock[CC_GOOD_GOLD] = 1;
    cult.goblins.lair_stock[CC_GOOD_GEMS] = 1;
    cult.dragon.hoard = 0;
    CcMoney cult_coins = cult.goblins.lair_coins;
    CcMoney cult_gold = CcSimTrackedGold(&cult);
    int32_t cult_gold_goods = CcSimTrackedGood(&cult, CC_GOOD_GOLD);
    int32_t cult_gems = CcSimTrackedGood(&cult, CC_GOOD_GEMS);
    CcSimAdvanceDays(&cult, 1);
    CC_CHECK(cult.dragon.egg_count == 0);
    CC_CHECK(cult.dragon.brood_days_remaining == 0);
    CC_CHECK(cult.dragon.hoard == 0);
    CC_CHECK(cult.goblins.lair_coins == cult_coins);
    CC_CHECK(CcSimTrackedGood(&cult, CC_GOOD_GOLD) == cult_gold_goods);
    CC_CHECK(CcSimTrackedGood(&cult, CC_GOOD_GEMS) == cult_gems);
    CC_CHECK(CountEvents(&cult, CC_EVENT_GOBLIN_DRAGON_SEED) == 0);
    CC_CHECK(cult.goblins.dragon_seed_phase ==
             CC_GOBLIN_DRAGON_SEED_RUMORED);
    CC_CHECK(cult.goblins.dragon_seed_days_remaining == 20 * 365);
    CC_CHECK(CountEvents(
        &cult, CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED) == 1);

    cult.current_day = 121 * 365;
    cult.dragon.afterdeath_days = 121 * 365 - 1;
    cult.goblins.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_PREPARING;
    cult.goblins.dragon_seed_days_remaining = 365;
    CcSimAdvanceDays(&cult, 1);
    CC_CHECK(cult.dragon.egg_count == 2);
    CC_CHECK(cult.dragon.brood_days_remaining >= 10 * 365 - 1);
    CC_CHECK(cult.dragon.brood_days_remaining <= 15 * 365 - 1);
    CC_CHECK(cult.dragon.hoard == 120);
    CC_CHECK(CcSimTrackedGold(&cult) == cult_gold);
    CC_CHECK(CcSimTrackedGood(&cult, CC_GOOD_GOLD) == cult_gold_goods);
    CC_CHECK(CcSimTrackedGood(&cult, CC_GOOD_GEMS) == cult_gems);
    CC_CHECK(CountEvents(&cult, CC_EVENT_GOBLIN_DRAGON_SEED) == 1);
    CC_CHECK(CcSimValidate(&cult, error, sizeof(error)));

    CcSim offerings;
    CcSimInit(&offerings, UINT32_C(0x0ffe7106));
    offerings.dragon.slain = true;
    offerings.dragon.slain_day = 1;
    offerings.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    offerings.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    offerings.dragon.body_condition = 0;
    offerings.dragon.crown_strength = 0;
    offerings.goblins.dragon_seed_phase =
        CC_GOBLIN_DRAGON_SEED_PREPARING;
    offerings.goblins.dragon_seed_days_remaining = 0;
    offerings.goblins.members = 84;
    offerings.goblins.devotion = 90;
    offerings.goblins.cohesion = 90;
    offerings.goblins.lair_coins = 0;
    offerings.goblins.lair_stock[CC_GOOD_FOOD] = 24;
    offerings.goblins.lair_stock[CC_GOOD_TOOLS] = 2;
    offerings.goblins.lair_stock[CC_GOOD_WEAPONS] = 3;
    offerings.goblins.lair_stock[CC_GOOD_GOLD] = 0;
    offerings.goblins.lair_stock[CC_GOOD_GEMS] = 0;
    for (int32_t i = 0; i < offerings.settlement_count; ++i) {
        offerings.settlements[i].stock[CC_GOOD_GOLD] += 1;
    }
    for (int32_t offering = 1; offering <= 4; ++offering) {
        offerings.current_day = (190 + offering * 10) * 365 - 1;
        offerings.dragon.afterdeath_days =
            (190 + offering * 10) * 365 - 1;
        CcSimAdvanceDays(&offerings, 1);
    }
    CC_CHECK(offerings.dragon.egg_count >= 1);
    CC_CHECK(offerings.goblins.lair_coins == 0);
    CC_CHECK(CountEvents(
        &offerings, CC_EVENT_GOBLIN_DRAGON_SEED) == 1);

    CcSim living_cult;
    CcSimInit(&living_cult, UINT32_C(0xc0171a1e));
    living_cult.current_day = 2 * 365 - 1;
    living_cult.goblins.members = 24;
    living_cult.goblins.devotion = 90;
    living_cult.goblins.lair_stock[CC_GOOD_FOOD] = 32;
    living_cult.goblins.lair_stock[CC_GOOD_TOOLS] = 3;
    living_cult.goblins.lair_stock[CC_GOOD_WEAPONS] = 4;
    living_cult.goblins.tribute_cooldown_days = 1000;
    CcSimAdvanceDays(&living_cult, 1);
    CC_CHECK(living_cult.goblins.members == 25);
    CC_CHECK(CountEvents(
        &living_cult, CC_EVENT_GOBLIN_CULT_RALLIED) == 1);

    CcSim ash_poor_cult;
    CcSimInit(&ash_poor_cult, UINT32_C(0xc017a500));
    ash_poor_cult.current_day = 4 * 365 - 1;
    ash_poor_cult.goblins.members = 12;
    ash_poor_cult.goblins.devotion = 100;
    ash_poor_cult.goblins.lair_stock[CC_GOOD_FOOD] = 16;
    ash_poor_cult.goblins.lair_stock[CC_GOOD_TOOLS] = 0;
    ash_poor_cult.goblins.lair_stock[CC_GOOD_WEAPONS] = 0;
    ash_poor_cult.goblins.tribute_cooldown_days = 1000;
    CcSimAdvanceDays(&ash_poor_cult, 1);
    CC_CHECK(ash_poor_cult.goblins.members == 13);

    CcSim stages;
    CcSimInit(&stages, UINT32_C(0x57a6e500));
    stages.dragon.memory_integrity = 20;
    stages.dragon.brood_cooldown_days = 1000;
    CcSimAdvanceDays(&stages, 1);
    CC_CHECK(stages.dragon.life_stage == CC_DRAGON_STAGE_UNCROWNED);
    CC_CHECK(CountEvents(&stages, CC_EVENT_DRAGON_UNCROWNED) == 1);

    stages.dragon.age_days = 60 * 365;
    stages.dragon.hoard = 5000;
    stages.dragon.hoard_goods[CC_GOOD_GOLD] = 10;
    stages.dragon.hoard_goods[CC_GOOD_GEMS] = 10;
    stages.dragon.memory_integrity = 100;
    stages.dragon.territory_stability = 100;
    stages.goblins.devotion = 100;
    CcSimAdvanceDays(&stages, 1);
    CC_CHECK(stages.dragon.life_stage == CC_DRAGON_STAGE_CROWNED);

    stages.dragon.age_days = 500 * 365;
    stages.dragon.crown_continuity_days = 200 * 365;
    stages.dragon.territory_stability = 100;
    stages.dragon.memory_integrity = 100;
    stages.dragon.brood_cooldown_days = 1000;
    CcSimAdvanceDays(&stages, 1);
    CC_CHECK(stages.dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM);

    stages.dragon.life_stage = CC_DRAGON_STAGE_UNCROWNED;
    stages.dragon.age_days = 20 * 365;
    stages.dragon.body_condition = 20;
    stages.dragon.hunt_cooldown_days = 0;
    stages.dragon.memory_integrity = 50;
    CcSimAdvanceDays(&stages, 1);
    CC_CHECK(stages.dragon.hunts == 1);
    CC_CHECK(stages.dragon.hunt_cooldown_days >= 14);
    CC_CHECK(stages.dragon.hunt_cooldown_days <= 42);

    CcSim strength;
    CcSimInit(&strength, UINT32_C(0x57a3e67a));
    strength.dragon.body_condition = 80;
    strength.dragon.crown_strength = 60;
    strength.dragon.memory_integrity = 80;
    strength.dragon.territory_stability = 80;
    strength.dragon.life_stage = CC_DRAGON_STAGE_UNCROWNED;
    int32_t uncrowned_strength = CcSimDragonBattleStrength(&strength);
    strength.dragon.life_stage = CC_DRAGON_STAGE_CROWNED;
    int32_t crowned_strength = CcSimDragonBattleStrength(&strength);
    strength.dragon.life_stage = CC_DRAGON_STAGE_DEEP_WYRM;
    int32_t deep_strength = CcSimDragonBattleStrength(&strength);
    CC_CHECK(uncrowned_strength < crowned_strength);
    CC_CHECK(crowned_strength < deep_strength);
    strength.dragon.crown_strength = 0;
    int32_t crownless_strength = CcSimDragonBattleStrength(&strength);
    strength.dragon.crown_strength = 120;
    CC_CHECK(CcSimDragonBattleStrength(&strength) ==
             crownless_strength + 10);

    const CcRoute *dragon_road = NULL;
    for (int32_t i = 0; i < strength.route_count; ++i) {
        if (strength.routes[i].from_id == strength.dragon.lair_settlement_id ||
            strength.routes[i].to_id == strength.dragon.lair_settlement_id) {
            dragon_road = &strength.routes[i];
            break;
        }
    }
    CC_CHECK(dragon_road != NULL);
    strength.player.location_id = strength.dragon.lair_settlement_id;
    CcId road_destination = dragon_road->from_id ==
            strength.player.location_id ? dragon_road->to_id :
            dragon_road->from_id;
    for (int32_t i = 0; i < strength.map_count; ++i) {
        if (strength.maps[i].route_id == dragon_road->id) {
            strength.maps[i].owner_id = strength.player.id;
            strength.player.map_catalogue_mask |= UINT32_C(1) << i;
            strength.player.map_archive_mask &= ~(UINT32_C(1) << i);
            break;
        }
    }
    CcTravelPreview quiet_preview = {0};
    CcTravelPreview shadow_preview = {0};
    strength.dragon.regional_influence = 0;
    CC_CHECK(CcSimTravelPreview(&strength, road_destination,
                                &quiet_preview, error, sizeof(error)));
    strength.dragon.regional_influence = 96;
    CC_CHECK(CcSimTravelPreview(&strength, road_destination,
                                &shadow_preview, error, sizeof(error)));
    CC_CHECK(shadow_preview.claimed_danger ==
             quiet_preview.claimed_danger + 2);

    puts("Dragon crown, cult recovery, brood, aftermath, and succession tests passed");
    return 0;
}
