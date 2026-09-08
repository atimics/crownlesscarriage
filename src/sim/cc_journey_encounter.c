#include "sim/cc_journey_internal.h"

#include <stdio.h>

static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }

static int32_t RollD6(CcSim *sim,
                       const CcJourneyEncounterServices *services)
{
    return 1 + (int32_t)(services->next_random(sim) % 6U);
}


static void RollEncounterLoot(CcSim *sim, CcBanditGroup *bandits,
                              const CcJourneyEncounter *journey,
                              CcId combat_event_id,
                              const CcJourneyEncounterServices *services)
{
    const int32_t roll = RollD6(sim, services) + RollD6(sim, services);
    if (bandits == NULL || roll < 4) return;

    CcGood good = CC_GOOD_FOOD;
    if (bandits->raid_phase == CC_BANDIT_RAID_RETURNING &&
        bandits->raid_good >= 0 && bandits->raid_good < CC_GOOD_COUNT) {
        good = bandits->raid_good;
    }
    const int32_t recoverable = bandits->supplies / 10;
    int32_t take = 0;
    if (roll <= 5) {
        take = recoverable * RollD6(sim, services) / 10;
    } else if (roll <= 8) {
        take = recoverable / 2;
    } else {
        take = recoverable;
    }

    int32_t free_slots = sim->player.cargo_capacity -
                         CcPlayerCargoUsed(&sim->player);
    if (free_slots < 0) free_slots = 0;
    const int32_t cap = free_slots;
    if (take > cap) take = cap;

    char text[CC_EVENT_TEXT_CAPACITY];
    size_t length = 0;
    int32_t magnitude = 0;
    CcTreasure *trophy = NULL;

    if (take > 0) {
        sim->player.cargo[good] += take;
        bandits->supplies = ClampI32(bandits->supplies - take * 10, 0, 100);
        magnitude += take;
        length += (size_t)snprintf(
            text + length, sizeof(text) - length,
            "The company strips the broken cordon: %d %s recovered",
            take, CcGoodName(good));
    }

    if (roll >= 12) {
        if (take == 0) {
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "The broken cordon yields little");
        }
        if (CcPlayerCargoUsed(&sim->player) <
                sim->player.cargo_capacity &&
            sim->treasure_count < CC_MAX_TREASURES) {
            trophy = services->allocate_treasure(sim);
        }
        if (trophy != NULL) {
            const int32_t value = RollD6(sim, services) + RollD6(sim, services);
            (void)snprintf(trophy->name, sizeof(trophy->name),
                           "Outlaw Trophy of %.16s", bandits->name);
            trophy->maker_settlement_id = sim->player.location_id;
            trophy->owner_id = sim->player.id;
            trophy->location_id = sim->player.location_id;
            trophy->gold_content = 1;
            trophy->gem_content = 1;
            trophy->craft_work = 1;
            trophy->appraised_value = value;
            trophy->created_day = sim->current_day;
            sim->player.treasure_cargo_slots += 1;
            magnitude += value;
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "%sthe outlaw trophy %s (%d crowns)",
                length > 0 && text[length - 1] != ' ' ? ", " : "",
                trophy->name, value);
        } else if (length > 0 && length < sizeof(text) - 1) {
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "; the trophy is left on the road");
        }
    }

    if (length > 0) {
        (void)snprintf(text + length, sizeof(text) - length, ".");
        (void)services->record_event(sim, CC_EVENT_ENCOUNTER_LOOT, journey->situation_id,
                        journey->route_id, combat_event_id,
                        magnitude, text);
    }
}

static bool ApplyResolveEncounter(CcSim *sim, CcJourneyOutcome outcome,
                                  bool provisions,
                                  char *error, size_t error_capacity,
                                  const CcJourneyEncounterServices *services)
{
    if (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
        (outcome != CC_JOURNEY_OUTCOME_COMBAT &&
         outcome != CC_JOURNEY_OUTCOME_NEGOTIATED)) {
        SetError(error, error_capacity, "There is no unresolved road encounter.");
        return false;
    }
    CcJourneyEncounter journey = sim->journey;
    CcRoute *route = services->route(sim, journey.route_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, journey.origin_id);
    CcSettlement *destination = CcSimSettlementMutable(
        sim, journey.destination_id);
    CcBanditGroup *bandits = services->bandits(sim, journey.route_id);
    if (route == NULL || origin == NULL || destination == NULL ||
        sim->player.location_id != journey.origin_id) {
        SetError(error, error_capacity,
                 "The road encounter no longer matches the prepared journey.");
        return false;
    }
    if (!provisions && outcome == CC_JOURNEY_OUTCOME_NEGOTIATED &&
        sim->player.coins < journey.bargain_cost) {
        SetError(error, error_capacity,
                 "The company cannot cover the negotiated passage.");
        return false;
    }
    CcGood demanded_good = CC_GOOD_FOOD;
    int32_t demanded_quantity = 0;
    if (provisions &&
        (!CcSimBanditProvisionDemand(sim, journey.route_id,
                                     &demanded_good, &demanded_quantity) ||
         sim->player.cargo[demanded_good] < demanded_quantity)) {
        SetError(error, error_capacity,
                 "The carriage does not carry the provisions they asked for.");
        return false;
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    CcEventKind event_kind;
    int32_t magnitude;
    if (outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        int32_t damage = ClampI32(7 + journey.danger / 8, 8, 20);
        int32_t medical_cost = MinimumI32(
            (int32_t)sim->player.coins, 3 + journey.danger / 12);
        sim->carriage.condition = ClampI32(
            sim->carriage.condition - damage, 0, 100);
        sim->player.coins -= medical_cost;
        origin->market_coins += medical_cost;
        route->security = ClampI32(route->security + 6, 0, 100);
        destination->security = ClampI32(destination->security + 2, 0, 100);
        if (bandits != NULL) {
            bandits->members = ClampI32(bandits->members - 3, 0, 200);
            bandits->supplies = ClampI32(bandits->supplies - 4, 0, 100);
            bandits->influence = ClampI32(bandits->influence - 3, 0, 100);
        }
        (void)snprintf(text, sizeof(text),
                       "The company breaks the cordon, but the carriage takes %d damage and wounds cost %d crowns.",
                       damage, medical_cost);
        event_kind = CC_EVENT_ENCOUNTER_COMBAT;
        magnitude = damage;
    } else if (!provisions) {
        sim->player.coins -= journey.bargain_cost;
        destination->market_coins += journey.bargain_cost;
        route->security = ClampI32(route->security - 1, 0, 100);
        destination->prosperity = ClampI32(destination->prosperity + 1, 0, 100);
        if (bandits != NULL) {
            bandits->supplies = ClampI32(bandits->supplies + 4, 0, 100);
            bandits->influence = ClampI32(bandits->influence + 3, 0, 100);
        }
        (void)snprintf(text, sizeof(text),
                       "The Crownless company buys passage for %d crowns; commerce moves immediately, but the collectors grow stronger.",
                       journey.bargain_cost);
        event_kind = CC_EVENT_ENCOUNTER_NEGOTIATED;
        magnitude = journey.bargain_cost;
    } else {
        int32_t supply_value = demanded_good == CC_GOOD_WEAPONS ? 4 :
                               demanded_good == CC_GOOD_TOOLS ? 3 :
                               demanded_good == CC_GOOD_IRON ? 2 : 1;
        sim->player.cargo[demanded_good] -= demanded_quantity;
        route->security = ClampI32(route->security - 1, 0, 100);
        if (bandits != NULL) {
            bandits->supplies = ClampI32(
                bandits->supplies + demanded_quantity * supply_value,
                0, 100);
            bandits->influence = ClampI32(bandits->influence + 1, 0, 100);
            if ((bandits->raid_phase == CC_BANDIT_RAID_SCOUTING ||
                 bandits->raid_phase == CC_BANDIT_RAID_MUSTERING) &&
                bandits->raid_good == demanded_good) {
                bandits->raid_phase = CC_BANDIT_RAID_IDLE;
                bandits->raid_target_id = 0U;
                bandits->raid_quantity = 0;
                bandits->raid_days_remaining = 0;
            }
        }
        (void)snprintf(
            text, sizeof(text),
            "The Crownless company gives %d %s to %s; their immediate shortage eases and the cordon opens.",
            demanded_quantity, CcGoodName(demanded_good),
            bandits != NULL ? bandits->name : "the road company");
        event_kind = CC_EVENT_ENCOUNTER_NEGOTIATED;
        magnitude = demanded_quantity;
    }
    CcEvent *outcome_event = services->record_event(
        sim, event_kind, journey.situation_id, journey.route_id,
        journey.parent_event_id, magnitude, text);
    CcId outcome_event_id = outcome_event->id;
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->journey.parent_event_id = outcome_event_id;
    if (outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        RollEncounterLoot(sim, bandits, &journey, outcome_event_id, services);
    }
    sim->resolved_journey_situation_id = journey.situation_id;
    sim->resolved_journey_outcome = outcome;
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second =
        CcJourneyCarriageSpeedForPace(
            sim->journey.total_subticks, sim->journey.pace);
    services->create_traffic(sim, &journey, outcome_event_id);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyWithdrawEncounter(CcSim *sim, const CcCommand *command,
                                   char *error, size_t error_capacity,
                                  const CcJourneyEncounterServices *services)
{
    if (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
        (command->amount != 0 && command->amount != 1)) {
        SetError(error, error_capacity,
                 "There is no road fight to withdraw from.");
        return false;
    }
    CcJourneyEncounter journey = sim->journey;
    CcRoute *route = services->route(sim, journey.route_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, journey.origin_id);
    CcBanditGroup *bandits = services->bandits(sim, journey.route_id);
    if (route == NULL || origin == NULL ||
        sim->player.location_id != journey.origin_id) {
        SetError(error, error_capacity,
                 "The road withdrawal no longer matches this journey.");
        return false;
    }
    bool under_fire = command->amount == 1;
    int32_t damage = under_fire ? ClampI32(4 + journey.danger / 15,
                                           4, 10) : 0;
    int32_t medical_cost = under_fire ? MinimumI32(
        (int32_t)sim->player.coins, 2 + journey.danger / 20) : 0;
    sim->carriage.condition = ClampI32(
        sim->carriage.condition - damage, 0, 100);
    sim->player.coins -= medical_cost;
    origin->market_coins += medical_cost;
    route->security = ClampI32(route->security - (under_fire ? 2 : 1),
                               0, 100);
    if (bandits != NULL) {
        bandits->influence = ClampI32(
            bandits->influence + (under_fire ? 2 : 1), 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    if (under_fire) {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage withdraws under fire to %s; it takes %d damage and treatment costs %d crowns.",
            origin->name, damage, medical_cost);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage refuses the fight and returns to %s before blood is drawn.",
            origin->name);
    }
    (void)services->record_event(sim, CC_EVENT_ENCOUNTER_WITHDRAWN, sim->player.id,
                    journey.route_id, journey.parent_event_id,
                    under_fire ? damage : 0, text);
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    sim->journey.active = false;
    sim->journey.phase = CC_JOURNEY_PHASE_NONE;
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = origin->id,
        .condition = sim->carriage.condition
    };
    SetError(error, error_capacity, "");
    return true;
}

bool CcJourneyResolveEncounter(CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity,
    const CcJourneyEncounterServices *services)
{
    switch (command->kind) {
        case CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_COMBAT,
                false, error, error_capacity, services);
        case CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_NEGOTIATED,
                false, error, error_capacity, services);
        case CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_NEGOTIATED,
                true, error, error_capacity, services);
        case CC_COMMAND_WITHDRAW_ENCOUNTER:
            return ApplyWithdrawEncounter(sim, command, error,
                error_capacity, services);
        default:
            SetError(error, error_capacity, "Encounter command is invalid.");
            return false;
    }
}
