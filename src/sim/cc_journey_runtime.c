#include "sim/cc_journey_internal.h"
#include "sim/cc_mine.h"
#include "sim/cc_road_position.h"

#include <stdio.h>

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }

static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void InterruptJourney(CcSim *sim,
    const CcJourneyRuntimeServices *services);

static void WarnJourneyAmbush(CcSim *sim,
    const CcJourneyRuntimeServices *services)
{
    if (sim == NULL || !sim->journey.active ||
        !sim->journey.ambush_pending || sim->journey.ambush_warned) return;
    const CcBanditGroup *bandits = services->bandits(
        sim, sim->journey.route_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "Scouts spot riders from %.24s shadowing the carriage. Careful pace may lose them before the road narrows.",
        bandits != NULL ? bandits->name : "a roadside company");
    CcEvent *event = services->record_event(
        sim, CC_EVENT_JOURNEY_WARNING, sim->player.id,
        sim->journey.route_id, sim->journey.parent_event_id,
        sim->journey.danger, text);
    sim->journey.parent_event_id = event->id;
    sim->journey.ambush_warned = true;
}

static void ResolveWarnedJourneyAmbush(CcSim *sim,
    const CcJourneyRuntimeServices *services)
{
    if (sim == NULL || !sim->journey.active ||
        !sim->journey.ambush_pending || !sim->journey.ambush_warned) return;
    if (sim->journey.pace == CC_JOURNEY_PACE_CAREFUL) {
        const CcBanditGroup *bandits = services->bandits(
            sim, sim->journey.route_id);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "The careful team leaves %.24s behind on a watched side track; no cargo or crowns are lost.",
            bandits != NULL ? bandits->name : "the roadside riders");
        CcEvent *event = services->record_event(
            sim, CC_EVENT_AMBUSH_EVADED, sim->player.id,
            sim->journey.route_id, sim->journey.parent_event_id, 0, text);
        sim->journey.parent_event_id = event->id;
        sim->journey.ambush_pending = false;
        sim->journey.ambush_resolved = true;
        return;
    }
    sim->journey.ambush_pending = false;
    sim->journey.ambush_resolved = true;
    sim->journey.encounter_triggered = true;
    InterruptJourney(sim, services);
}

static void FinishJourneyArrival(CcSim *sim,
    const CcJourneyRuntimeServices *services)
{
    if (sim == NULL || !sim->journey.active) return;
    const CcSettlement *destination = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->journey.origin_id);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (destination == NULL || route == NULL) return;
    if (sim->journey.situation_id != 0U &&
        sim->player.accepted_situation_id == sim->journey.situation_id) {
        const CcSituation *situation = CcSimSituation(
            sim, sim->journey.situation_id);
        bool delivery = situation != NULL &&
            (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
             situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
        int32_t remaining = situation != NULL ?
            situation->quantity - situation->progress : 0;
        bool delivered_load_arrived = !delivery ||
            (destination->id == situation->target_id &&
             situation->good >= 0 && situation->good < CC_GOOD_COUNT &&
             sim->player.cargo[situation->good] >= remaining);
        if (delivered_load_arrived) {
            if (sim->resolved_journey_situation_id !=
                sim->journey.situation_id) {
                sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
            }
            sim->resolved_journey_situation_id = sim->journey.situation_id;
        } else {
            sim->resolved_journey_situation_id = 0U;
            sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
        }
    }
    sim->player.location_id = destination->id;
    services->exchange_gossip(sim, sim->player.id, destination->id, "Your fellow travelers");
    services->reveal_complete_route(sim, route->id);
    services->reveal_settlement_roads(sim, destination->id);
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed &&
            treasure->owner_id == sim->player.id) {
            treasure->location_id = destination->id;
        }
    }
    if (!sim->journey.road_position_active)
        sim->journey.elapsed_subticks = sim->journey.total_subticks;
    sim->journey.road_position_active = false;
    sim->journey.road_waiting_choice = false;
    sim->journey.active = false;
    sim->journey.phase = CC_JOURNEY_PHASE_NONE;
    CcSimPeopleEnterSettlement(sim);
    sim->clock.game_minutes_per_second =
        CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = destination->id,
        .condition = sim->carriage.condition
    };
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourier *courier = &sim->couriers[i];
        if (courier->status != CC_COURIER_WITH_PLAYER) continue;
        courier->current_settlement_id = destination->id;
        services->exchange_gossip(sim, courier->id, destination->id, "Royal couriers");
        courier->reliability = ClampI32(
            courier->reliability - sim->journey.danger / 12, 0, 100);
        if (courier->destination_settlement_id == destination->id) {
            services->deliver_courier(sim, courier, true);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    int32_t journey_watches = sim->journey.road_position_active ?
        (sim->journey.elapsed_subticks + CC_WORLD_WATCH_SUBTICKS - 1) /
            CC_WORLD_WATCH_SUBTICKS :
        sim->journey.total_subticks / CC_WORLD_WATCH_SUBTICKS;
    (void)snprintf(text, sizeof(text),
                   "The carriage reaches %.24s from %.24s after %d road watches at %s pace (%d%% danger).",
                   destination->name,
                   origin != NULL ? origin->name : "the road",
                   journey_watches,
                   CcJourneyPaceName(sim->journey.pace),
                   sim->journey.danger);
    (void)services->record_event(sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id,
                    destination->id, sim->journey.parent_event_id,
                    (journey_watches + 1) / 2,
                    text);
    services->deliver_delayed_echo(sim);
}

static void InterruptJourney(CcSim *sim,
    const CcJourneyRuntimeServices *services)
{
    const CcSettlement *destination = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcSituation *situation = CcSimSituation(
        sim, sim->journey.situation_id);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    const CcBanditGroup *bandits = services->bandits(
        sim, sim->journey.route_id);
    if (sim->schema_version >= 111U && bandits != NULL &&
        bandits->members <= 0) bandits = NULL;
    char text[CC_EVENT_TEXT_CAPACITY];
    if (route != NULL && route->closed && bandits == NULL &&
        (sim->schema_version < 111U ||
         CcJourneyAlderwatchChain(sim, route, sim->journey.origin_id,
                                   sim->journey.destination_id))) {
        (void)snprintf(
            text, sizeof(text),
            "Captain Ilyra Senn lowers Alderwatch's bright chain across the road. 'Orders,' she says, while a hungry boy eats too quickly on the wall.");
    } else if (sim->schema_version >= 111U && bandits == NULL) {
        (void)snprintf(text, sizeof(text),
                       "The carriage stops near %.24s. The route is closed, and no road collectors are present.",
                       destination != NULL ? destination->name : "the far gate");
    } else if (situation == NULL) {
        (void)snprintf(
            text, sizeof(text),
            "The warned riders from %.24s close the road to %.24s. The carriage stops before anything is taken.",
            bandits != NULL ? bandits->name : "an armed roadside company",
            destination != NULL ? destination->name : "the far gate");
    } else {
        (void)snprintf(text, sizeof(text),
                       "Members of %.24s block the road to %.24s; %.24s awaits a Crownless choice.",
                       bandits != NULL ? bandits->name : "Armed road collectors",
                       destination != NULL ? destination->name : "the far gate",
                       situation->affected_name[0] != '\0' ?
                           situation->affected_name : "a local household");
    }
    CcEvent *event = services->record_event(
        sim, CC_EVENT_JOURNEY_ENCOUNTER, sim->journey.situation_id,
        sim->journey.route_id, sim->journey.parent_event_id,
        sim->journey.danger, text);
    sim->journey.parent_event_id = event->id;
    sim->journey.encounter_triggered = true;
    sim->journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    sim->clock.game_minutes_per_second = 0;
    sim->carriage.mode = CC_CARRIAGE_STOPPED;
    sim->carriage.speed_milli_per_second = 0;
}

static void PauseJourneyForWatchStop(CcSim *sim,
    const CcJourneyRuntimeServices *services)
{
    if (sim->journey.ambush_pending && !sim->journey.ambush_warned) {
        WarnJourneyAmbush(sim, services);
    }
    sim->journey.phase = CC_JOURNEY_PHASE_RESTING;
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_STOPPED;
    sim->carriage.speed_milli_per_second = 0;
}

void CcJourneyAdvanceTicks(CcSim *sim, int32_t ticks,
    const CcJourneyRuntimeServices *services)
{
    if (sim == NULL || ticks <= 0 || sim->mine.phase != CC_MINE_NONE || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
        sim->clock.tick > UINT64_MAX - (uint64_t)ticks) return;
    for (int32_t tick = 0; tick < ticks; ++tick) {
        int32_t mine_stop=CcMineBranchSubtick(sim);
        if (mine_stop >= 0 && sim->journey.elapsed_subticks == mine_stop) break;
        if (sim->schema_version >= 40U && sim->pony_company.encounter >= 0) break;
        if (!sim->journey.active ||
            sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) break;
        sim->clock.tick += 1U;
        int32_t clock_rate = CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        int32_t journey_rate = CcJourneyPaceRate(sim->journey.pace);
        sim->clock.game_minutes_per_second = clock_rate;
        int32_t next_watch =
            (sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS + 1) *
            CC_WORLD_WATCH_SUBTICKS;
        int32_t next_limit = sim->journey.road_position_active ? next_watch :
            MinimumI32(sim->journey.total_subticks, next_watch);
        if (mine_stop > sim->journey.elapsed_subticks)
            next_limit = MinimumI32(next_limit, mine_stop);
        int32_t previous_elapsed = sim->journey.elapsed_subticks;
        int32_t advance = MinimumI32(
            journey_rate, next_limit - sim->journey.elapsed_subticks);
        if (sim->journey.road_position_active) {
            advance = MinimumI32(
                advance, sim->journey.road_leg_total_subticks -
                    sim->journey.road_leg_elapsed_subticks);
            int32_t hazard = -1;
            if (sim->journey.ambush_pending) {
                int32_t warning = CcRoadSubticksUntilRouteProgress(
                    sim, sim->journey.total_subticks * 45 / 100);
                int32_t ambush = CcRoadSubticksUntilRouteProgress(
                    sim, sim->journey.total_subticks * 60 / 100);
                if (!sim->journey.ambush_warned && warning >= 0)
                    hazard = warning;
                if (ambush >= 0 && (hazard < 0 || ambush < hazard))
                    hazard = ambush;
            }
            if (!sim->journey.encounter_triggered &&
                sim->journey.situation_id != 0U) {
                int32_t encounter = CcRoadSubticksUntilRouteProgress(
                    sim, sim->journey.encounter_subticks);
                if (encounter >= 0 && (hazard < 0 || encounter < hazard))
                    hazard = encounter;
            }
            if (hazard >= 0) advance = MinimumI32(advance, hazard);
        }
        if (advance < 0) advance = 0;
        int32_t clock_advance = sim->journey.road_position_active ?
            (int32_t)(((int64_t)advance * clock_rate + journey_rate / 2) /
                      journey_rate) : clock_rate;
        sim->clock.minute_subticks += clock_advance;
        while (sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS) {
            sim->clock.minute_subticks -= CC_WORLD_DAY_SUBTICKS;
            CcSimAdvanceDays(sim, 1);
        }
        sim->journey.elapsed_subticks += advance;
        if (!sim->journey.road_position_active)
            sim->carriage.progress_milli = sim->journey.total_subticks > 0 ?
            (int32_t)(((int64_t)sim->journey.elapsed_subticks * 1000) /
                      sim->journey.total_subticks) : 0;
        bool road_arrived = sim->journey.road_position_active &&
            advance > 0 && CcRoadAdvanceLeg(sim, advance);
        services->reveal_journey_road(sim);
        if (mine_stop >= 0 && sim->journey.elapsed_subticks == mine_stop) break;
        if (!sim->journey.road_position_active &&
            sim->journey.elapsed_subticks >= sim->journey.total_subticks) {
            CcJourneyApplyWatchStrain(sim);
            FinishJourneyArrival(sim, services);
            continue;
        }
        int32_t route_progress = sim->journey.road_position_active ?
            CcRoadRouteProgressSubticks(sim) :
            sim->journey.elapsed_subticks;
        if (sim->journey.ambush_pending &&
            !sim->journey.ambush_warned &&
            route_progress >= sim->journey.total_subticks * 45 / 100) {
            WarnJourneyAmbush(sim, services);
        }
        if (sim->journey.ambush_pending &&
            route_progress >= sim->journey.total_subticks * 60 / 100) {
            ResolveWarnedJourneyAmbush(sim, services);
            if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) continue;
        }
        if (!sim->journey.encounter_triggered &&
            sim->journey.situation_id != 0U &&
            route_progress >= sim->journey.encounter_subticks) {
            InterruptJourney(sim, services);
            continue;
        }
        if (road_arrived) {
            bool watch_boundary =
                sim->journey.elapsed_subticks != previous_elapsed &&
                sim->journey.elapsed_subticks == next_watch;
            if (watch_boundary) CcJourneyApplyWatchStrain(sim);
            if (sim->journey.road_anchor_id ==
                    sim->journey.road_goal_id &&
                CcSimSettlement(sim, sim->journey.road_anchor_id) != NULL) {
                FinishJourneyArrival(sim, services);
            } else if (watch_boundary) {
                PauseJourneyForWatchStop(sim, services);
            }
            continue;
        }
        if (sim->journey.elapsed_subticks != previous_elapsed &&
            sim->journey.elapsed_subticks == next_watch) {
            CcJourneyApplyWatchStrain(sim);
            PauseJourneyForWatchStop(sim, services);
            continue;
        }
    }
}
