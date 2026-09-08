#include "sim/cc_journey_internal.h"
#include "sim/cc_route_rules_internal.h"

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

static void SpoilPlayerJourneyCargo(CcSim *sim, bool rain_expected,
                                    int32_t *meat_spoiled,
                                    int32_t *grain_spoiled)
{
    if (sim == NULL || sim->schema_version < 33U) {
        *meat_spoiled = 0;
        *grain_spoiled = 0;
        return;
    }
    *meat_spoiled = sim->player.cargo[CC_GOOD_MEAT];
    *grain_spoiled = rain_expected ?
        sim->player.cargo[CC_GOOD_WHEAT] : 0;
    sim->player.cargo[CC_GOOD_MEAT] = 0;
    int64_t rotten_meat =
        (int64_t)sim->player.cargo[CC_GOOD_ROTTEN_MEAT] + *meat_spoiled;
    sim->player.cargo[CC_GOOD_ROTTEN_MEAT] =
        rotten_meat > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
                                         (int32_t)rotten_meat;
    if (*grain_spoiled > 0) {
        sim->player.cargo[CC_GOOD_WHEAT] = 0;
        int64_t rotten_grain =
            (int64_t)sim->player.cargo[CC_GOOD_ROTTEN_GRAIN] +
            *grain_spoiled;
        sim->player.cargo[CC_GOOD_ROTTEN_GRAIN] =
            rotten_grain > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
                                              (int32_t)rotten_grain;
    }
}

bool CcJourneyDepart(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity,
                        const CcJourneyDepartureServices *services)
{
    if (sim->journey.active) {
        SetError(error, error_capacity,
                 "Resolve the encounter already blocking the carriage.");
        return false;
    }
    CcTravelPreview preview = {0};
    if (!CcSimTravelPreview(sim, command->target_id, &preview,
                            error, error_capacity)) return false;
    const CcSettlement *destination = CcSimSettlement(
        sim, preview.destination_id);
    const CcRoute *route = CcSimRoute(sim, preview.route_id);
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    bool sponsored_night_passage = preview.sponsored_guide;
    bool delivery = accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
    bool full_contract_load = delivery &&
        sim->player.cargo[accepted->good] >=
            accepted->quantity - accepted->progress;
    bool sanctioned_closed_crossing = route->closed &&
        accepted != NULL &&
        accepted->kind == CC_SITUATION_RELIEF_DELIVERY &&
        full_contract_load;
    bool uncharted = !preview.charted && !sponsored_night_passage;
    int32_t days = preview.travel_days;
    /* Replay older journals with their original departure transfers. */
    int32_t base_fare = sim->schema_version >= 41U ? 0 :
        days + (route->smuggler_route ? 3 : 0);
    CcMoney toll = sim->schema_version >= 41U ? 0 :
        CcRouteToll(sim, route);
    CcMoney fare = preview.provision_cost;
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->player.location_id);
    if (sim->schema_version >= 14U && preview.horse_readiness < 30) {
        SetError(error, error_capacity,
                 "The horse team needs food and rest before another journey.");
        return false;
    }
    if (sim->schema_version >= 15U) {
        for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
            int32_t due = sim->horse_team[i].pregnancy_days_remaining;
            if (due > 0 && due <= 30) {
                SetError(error, error_capacity,
                         "A mare near foaling must remain at the stable.");
                return false;
            }
        }
    }
    if (sim->schema_version >= 14U && (origin == NULL ||
        CcNutritionAvailable(origin->stock, CC_NUTRITION_ANIMAL) <
            preview.horse_feed_required * CC_NUTRITION_PER_RATION)) {
        SetError(error, error_capacity,
                 "The departure market lacks enough fodder for the horse team.");
        return false;
    }
    if (sim->player.coins < fare) {
        SetError(error, error_capacity, "The company cannot provision that journey.");
        return false;
    }
    bool contract_journey = false;
    if (accepted != NULL && full_contract_load) {
        contract_journey = accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
            (accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
             route->smuggler_route);
    }
    if (accepted != NULL &&
        accepted->kind == CC_SITUATION_MONSTER_EXPEDITION &&
        destination->id == CcSimSituationOfferSettlementId(sim, accepted)) {
        contract_journey = true;
    }
    if (accepted != NULL &&
        accepted->kind == CC_SITUATION_COURIER_DELIVERY) {
        CcCourier *courier = services->courier(sim, accepted->target_id);
        if (courier != NULL &&
            courier->status == CC_COURIER_WITH_PLAYER) {
            contract_journey = true;
        }
    }
    bool encounter_planned = contract_journey &&
        (sanctioned_closed_crossing ||
         (accepted != NULL &&
          accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
          route->smuggler_route));
    int32_t danger = ClampI32(
        CcSimRouteDanger(sim, route->id) +
        CcRouteDragonShadowDanger(sim, route), 0, 95);
    if (uncharted) danger = ClampI32(danger + 20, 0, 95);
    int32_t reaction = CcSimBanditReactionRoll(sim, route->id);
    int32_t bargain_cost = ClampI32(
        4 + danger / 7 + (reaction <= 5 ? 3 : reaction >= 10 ? -2 : 0),
        3, 21);
    int32_t total_subticks = preview.travel_watches *
        CC_WORLD_WATCH_SUBTICKS;
    bool ambush_pending = !encounter_planned &&
        (int32_t)(services->next_random(sim) % 100U) < danger / 2;
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    sim->player.coins -= fare;
    CcSettlement *origin_market = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (origin_market != NULL) {
        origin_market->market_coins += base_fare;
        if (sim->schema_version >= 14U) {
            (void)CcNutritionConsume(
                origin_market->stock, CC_NUTRITION_ANIMAL,
                preview.horse_feed_required * CC_NUTRITION_PER_RATION);
        }
    }
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
            sim->horse_team[i].hunger = ClampI32(
                sim->horse_team[i].hunger -
                    preview.horse_feed_required * 12, 0, 100);
        }
    }
    CcKingdom *toll_kingdom = services->kingdom(sim, destination->kingdom_id);
    if (toll_kingdom != NULL) toll_kingdom->treasury += toll;
    bool waited_for_morning = preview.departure_wait_minutes > 0;
    if (waited_for_morning) {
        sim->clock.minute_subticks = 0;
        CcSimAdvanceDays(sim, 1);
    }
    int32_t meat_spoiled = 0;
    int32_t grain_spoiled = 0;
    SpoilPlayerJourneyCargo(
        sim, preview.rain_expected, &meat_spoiled, &grain_spoiled);
    services->exchange_gossip(sim, sim->player.id, sim->player.location_id, "Your fellow travelers");
    CcId parent_event_id = services->latest_local_cause(sim, destination->id);
    sim->journey = (CcJourneyEncounter){
        .active = true,
        .phase = CC_JOURNEY_PHASE_TRAVELLING,
        .situation_id = contract_journey ? accepted->id : 0U,
        .origin_id = sim->player.location_id,
        .destination_id = destination->id,
        .route_id = route->id,
        .danger = danger,
        .bargain_cost = bargain_cost,
        .departure_day = sim->current_day,
        .total_subticks = total_subticks,
        .encounter_subticks = encounter_planned ?
            total_subticks * 35 / 100 : 0,
        .fare_reserved = (int32_t)fare,
        .pace = CC_JOURNEY_PACE_STEADY,
        .ambush_pending = ambush_pending,
        .encounter_triggered = contract_journey && !encounter_planned,
        .parent_event_id = parent_event_id
    };
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_MOVING,
        .route_id = route->id,
        .origin_id = sim->player.location_id,
        .destination_id = destination->id,
        .speed_milli_per_second =
            CcJourneyCarriageSpeedForPace(
                total_subticks, CC_JOURNEY_PACE_STEADY),
        .condition = sim->carriage.condition
    };
    services->reveal_settlement_roads(sim, sim->journey.origin_id);
    services->reveal_journey_road(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    if (sim->schema_version >= 14U) {
        (void)snprintf(
            text, sizeof(text),
            "%.16s and %.16s pull from %.16s toward %.16s %sfor %d watches with %d fodder.",
            sim->schema_version >= 40U ? CcPonyName(sim->pony_company.team[0]) : sim->horse_team[0].name,
            sim->schema_version >= 40U ? CcPonyName(sim->pony_company.team[1]) : sim->horse_team[1].name,
            origin != NULL ? origin->name : "the waystation",
            destination->name,
            waited_for_morning ? "at first light " : "",
            preview.travel_watches,
            preview.horse_feed_required);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage leaves %s for %s with %d travel watches reserved.",
            origin != NULL ? origin->name : "the waystation",
            destination->name, preview.travel_watches);
    }
    CcEvent *departure = services->record_event(
        sim, CC_EVENT_JOURNEY_DEPARTED, sim->player.id, route->id,
        parent_event_id, days, text);
    sim->journey.parent_event_id = departure->id;
    if (meat_spoiled > 0 || grain_spoiled > 0) {
        if (meat_spoiled > 0 && grain_spoiled > 0) {
            (void)snprintf(
                text, sizeof(text),
                "Before the first mile, %d Meat becomes Rotten Meat; rain changes %d Wheat into Rotten Grain.",
                meat_spoiled, grain_spoiled);
        } else if (meat_spoiled > 0) {
            (void)snprintf(
                text, sizeof(text),
                "Before the first mile, %d Meat becomes Rotten Meat.",
                meat_spoiled);
        } else {
            (void)snprintf(
                text, sizeof(text),
                "Rain changes %d Wheat into Rotten Grain.", grain_spoiled);
        }
        CcEvent *spoilage = services->record_event(
            sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id, route->id,
            departure->id, meat_spoiled + grain_spoiled, text);
        sim->journey.parent_event_id = spoilage->id;
    }
    SetError(error, error_capacity, "");
    return true;
}

