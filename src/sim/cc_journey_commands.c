#include "sim/cc_journey_internal.h"

#include <stdio.h>

static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
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

static bool ApplyJourneyPace(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    if (sim == NULL || command == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) {
        SetError(error, error_capacity,
                 "Pace can only change while the carriage is moving.");
        return false;
    }
    if (command->amount < CC_JOURNEY_PACE_CAREFUL ||
        command->amount > CC_JOURNEY_PACE_PUSH) {
        SetError(error, error_capacity, "Journey pace is invalid.");
        return false;
    }
    sim->journey.pace = (CcJourneyPace)command->amount;
    sim->carriage.speed_milli_per_second = CcJourneyCarriageSpeedForPace(
        sim->journey.total_subticks, sim->journey.pace);
    SetError(error, error_capacity, "");
    return true;
}

void CcJourneyApplyWatchStrain(CcSim *sim)
{
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (route == NULL || sim->schema_version < 14U) return;
    int32_t cargo_strain = sim->player.cargo_capacity > 0 ?
        CcPlayerCargoUsed(&sim->player) * 3 /
            sim->player.cargo_capacity : 0;
    int32_t road_strain = MaximumI32(0, 60 - route->condition) / 18;
    int32_t pace_strain =
        sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 4 :
        sim->journey.pace == CC_JOURNEY_PACE_CAREFUL ? -2 : 0;
    int32_t road_wear = MaximumI32(0, 70 - route->condition) / 35;
    int32_t pace_wear =
        sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 2 :
        sim->journey.pace == CC_JOURNEY_PACE_STEADY ? 1 : 0;
    sim->carriage.condition = ClampI32(
        sim->carriage.condition - road_wear - pace_wear, 0, 100);
    for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
        CcHorse *horse = &sim->horse_team[i];
        int32_t strength_strain = sim->schema_version >= 15U ?
            cargo_strain * MaximumI32(50, 150 - horse->strength) / 100 :
            cargo_strain;
        int32_t strain = 4 + strength_strain + road_strain + pace_strain -
            horse->hardiness / 35;
        horse->fatigue = ClampI32(
            horse->fatigue + MaximumI32(1, strain), 0, 100);
        horse->hunger = ClampI32(horse->hunger + 1, 0, 100);
        if (horse->fatigue >= 88 || horse->hunger >= 80) {
            horse->health = ClampI32(horse->health - 1, 1, 100);
        }
    }
}

static void RecoverJourneyTeam(CcSim *sim, int32_t fatigue_recovery,
                               int32_t hunger_recovery)
{
    if (sim->schema_version < 14U) return;
    for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
        CcHorse *horse = &sim->horse_team[i];
        horse->fatigue = ClampI32(
            horse->fatigue - fatigue_recovery, 0, 100);
        horse->hunger = ClampI32(
            horse->hunger - hunger_recovery, 0, 100);
    }
}

static void ResumeJourney(CcSim *sim)
{
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second = CcJourneyCarriageSpeedForPace(
        sim->journey.total_subticks, sim->journey.pace);
}

static void AdvanceJourneyRestWatch(CcSim *sim)
{
    sim->clock.minute_subticks += CC_WORLD_WATCH_SUBTICKS;
    while (sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS) {
        sim->clock.minute_subticks -= CC_WORLD_DAY_SUBTICKS;
        CcSimAdvanceDays(sim, 1);
    }
}

static bool ApplyRoadSiteStop(CcSim *sim, const CcCommand *command,
                              char *error, size_t error_capacity,
                              CcJourneyRecordEvent record_event)
{
    const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
    if (site == NULL || site->id != command->target_id) {
        SetError(error, error_capacity, "Reach this roadside stop first.");
        return false;
    }
    int32_t slot = (int32_t)(site - sim->road_sites);
    sim->journey.road_site_stop_mask |= UINT32_C(1) << slot;
    bool camping = command->kind == CC_COMMAND_CAMP_ROAD_SITE;
    char text[CC_EVENT_TEXT_CAPACITY];
    if (camping) {
        RecoverJourneyTeam(sim, 8, 5);
        sim->journey.danger = ClampI32(sim->journey.danger + 3, 0, 95);
        AdvanceJourneyRestWatch(sim);
        (void)snprintf(text, sizeof(text),
            "The company camps beside %.40s for one watch. The team rests while a guard keeps watch by the carriage.",
            site->name);
    } else {
        (void)snprintf(text, sizeof(text),
            "The company passes the turn to %.40s and follows the main road.",
            site->name);
    }
    CcEvent *event = record_event(sim,
        camping ? CC_EVENT_JOURNEY_CAMP : CC_EVENT_JOURNEY_BREAK,
        sim->player.id, sim->journey.route_id,
        sim->journey.parent_event_id, camping ? 3 : 0, text);
    sim->journey.parent_event_id = event->id;
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyJourneyStopAction(CcSim *sim, const CcCommand *command,
                                   char *error, size_t error_capacity,
                              CcJourneyRecordEvent record_event)
{
    CcJourneyStopKind stop = CcSimJourneyStop(sim);
    if (sim == NULL || command == NULL || stop == CC_JOURNEY_STOP_NONE) {
        SetError(error, error_capacity,
                 "The carriage is not waiting at a travel stop.");
        return false;
    }
    bool midday = stop == CC_JOURNEY_STOP_MIDDAY;
    bool overnight = stop == CC_JOURNEY_STOP_OVERNIGHT;
    bool road_house = CcSimJourneyRoadHouseAvailable(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    CcEventKind event_kind = CC_EVENT_JOURNEY_BREAK;
    int32_t magnitude = 0;

    if (command->kind == CC_COMMAND_TAKE_JOURNEY_BREAK && midday) {
        RecoverJourneyTeam(sim, 2, 0);
        sim->journey.danger = ClampI32(sim->journey.danger - 2, 0, 95);
        (void)snprintf(
            text, sizeof(text),
            "The company waters the team, checks the wheels, and reads the road before the afternoon watch.");
    } else if (command->kind == CC_COMMAND_PRESS_ON && midday) {
        for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
            sim->horse_team[i].fatigue = ClampI32(
                sim->horse_team[i].fatigue + 4, 0, 100);
        }
        sim->journey.danger = ClampI32(sim->journey.danger + 5, 0, 95);
        magnitude = 5;
        (void)snprintf(
            text, sizeof(text),
            "The company presses through the midday stop. The team tires and the road grows harder to read.");
    } else if (command->kind == CC_COMMAND_MAKE_CAMP && overnight) {
        RecoverJourneyTeam(sim, 8, 5);
        sim->journey.danger = ClampI32(sim->journey.danger + 3, 0, 95);
        AdvanceJourneyRestWatch(sim);
        event_kind = CC_EVENT_JOURNEY_CAMP;
        magnitude = 3;
        (void)snprintf(
            text, sizeof(text),
            "The company makes camp, feeds the team from its reserved fodder, and keeps a lantern watch until morning.");
    } else if (command->kind == CC_COMMAND_LODGE_ROAD_HOUSE &&
               overnight && road_house) {
        CcMoney cost = CcSimRoadHouseCost(sim, sim->journey.route_id);
        if (sim->player.coins < cost) {
            SetError(error, error_capacity,
                     "The company cannot afford beds and stable feed here.");
            return false;
        }
        sim->player.coins -= cost;
        CcSettlement *origin = CcSimSettlementMutable(
            sim, sim->journey.origin_id);
        if (origin != NULL) origin->market_coins += cost;
        RecoverJourneyTeam(sim, 15, 10);
        sim->carriage.condition = ClampI32(
            sim->carriage.condition + 2, 0, 100);
        sim->journey.danger = ClampI32(sim->journey.danger - 5, 0, 95);
        sim->journey.ambush_pending = false;
        sim->journey.ambush_resolved = true;
        AdvanceJourneyRestWatch(sim);
        event_kind = CC_EVENT_ROAD_HOUSE_LODGING;
        magnitude = (int32_t)cost;
        (void)snprintf(
            text, sizeof(text),
            "The company pays %d crowns at %.32s. Warm beds, stable feed, and a wheelwright make the morning safer.",
            (int32_t)cost,
            CcSimRoadHouseName(sim, sim->journey.route_id));
    } else {
        SetError(error, error_capacity, midday ?
                 "Choose a midday break or press on." :
                 road_house ? "Choose camp or the road house." :
                              "Choose camp before the next watch.");
        return false;
    }

    CcEvent *event = record_event(
        sim, event_kind, sim->player.id, sim->journey.route_id,
        sim->journey.parent_event_id, magnitude, text);
    sim->journey.parent_event_id = event->id;
    ResumeJourney(sim);
    SetError(error, error_capacity, "");
    return true;
}

bool CcJourneyApplyCommand(CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity, CcJourneyRecordEvent record_event)
{
    switch (command->kind) {
        case CC_COMMAND_SET_JOURNEY_PACE:
            return ApplyJourneyPace(sim, command, error, error_capacity);
        case CC_COMMAND_TAKE_JOURNEY_BREAK:
        case CC_COMMAND_PRESS_ON:
        case CC_COMMAND_MAKE_CAMP:
        case CC_COMMAND_LODGE_ROAD_HOUSE:
            return ApplyJourneyStopAction(sim, command, error, error_capacity, record_event);
        case CC_COMMAND_CAMP_ROAD_SITE:
        case CC_COMMAND_PASS_ROAD_SITE:
            return ApplyRoadSiteStop(sim, command, error, error_capacity, record_event);
        default:
            SetError(error, error_capacity, "Journey command is invalid.");
            return false;
    }
}
