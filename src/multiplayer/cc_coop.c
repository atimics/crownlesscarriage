#include "multiplayer/cc_coop.h"
#include "multiplayer/cc_coop_commands.h"
#include "persistence/cc_save.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CcSim *CcCoopCreate(uint32_t seed)
{
    CcSim *sim = malloc(sizeof(*sim));
    if (sim != NULL) CcSimInit(sim, seed);
    return sim;
}

void CcCoopDestroy(CcSim *sim) { free(sim); }
void CcCoopFree(void *bytes) { CcSaveFreeBuffer(bytes); }

bool CcCoopEncode(const CcSim *sim, unsigned char **bytes, size_t *length,
                   char *error, size_t capacity)
{
    return CcSaveEncode(sim, bytes, length, error, capacity);
}

bool CcCoopDecode(CcSim *sim, const unsigned char *bytes, size_t length,
                   char *error, size_t capacity)
{
    return CcSaveDecode(bytes, length, sim, error, capacity);
}

bool CcCoopApply(CcSim *sim, const char *action, CcId target,
                  int32_t good, int32_t amount, char *error, size_t capacity)
{
    if (sim != NULL && action != NULL && strcmp(action, "skip_watch") == 0) {
        if (!sim->journey.active || sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) return false;
        CcSim *candidate = malloc(sizeof(*candidate));
        if (candidate == NULL) return false;
        *candidate = *sim;
        bool warned = candidate->journey.ambush_warned;
        for (int32_t tick = 0; tick < 3600 && candidate->journey.active &&
             candidate->journey.phase == CC_JOURNEY_PHASE_TRAVELLING; ++tick) {
            if (CcSimJourneyRoadSiteStop(candidate) != NULL) break;
            CcSimAdvanceRuntimeTicks(candidate, 1);
            if (!warned && candidate->journey.ambush_warned) break;
        }
        bool ok = CcSimValidate(candidate, error, capacity);
        if (ok) *sim = *candidate;
        free(candidate);
        return ok;
    }
    CcCommand command = { .target_id = target, .good = (CcGood)good, .amount = amount };
    if (action != NULL) {
        for (int32_t i = 1; i <= (int32_t)CC_COMMAND_PARTY_WIPE; ++i) {
            if (strcmp(action, CcCoopActionName((CcCommandKind)i)) == 0) command.kind = (CcCommandKind)i;
        }
    }
    if (command.kind == CC_COMMAND_CHANGE_DUNGEON) command.dungeon_state = (CcDungeonState)amount;
    if (sim == NULL || command.kind == CC_COMMAND_NONE ||
        good < 0 || good >= CC_GOOD_COUNT ||
        amount < -CC_SIM_MAX_UNITS || amount > CC_SIM_MAX_UNITS) {
        if (error != NULL && capacity > 0U) (void)snprintf(error, capacity, "Choose an available company action.");
        return false;
    }
    CcSim *candidate = malloc(sizeof(*candidate));
    if (candidate == NULL) return false;
    *candidate = *sim;
    bool ok = CcSimApply(candidate, &command, error, capacity) &&
              CcSimValidate(candidate, error, capacity);
    if (ok) *sim = *candidate;
    free(candidate);
    return ok;
}

bool CcCoopAdvanceAway(CcSim *sim, int32_t days, char *error, size_t capacity)
{
    if (sim == NULL || days < 0 || days > 365 ||
        days > CC_SIM_MAX_DAY - sim->current_day) return false;
    CcSim *candidate = malloc(sizeof(*candidate));
    if (candidate == NULL) return false;
    *candidate = *sim;
    CcSimAdvanceDays(candidate, days);
    bool ok = CcSimValidate(candidate, error, capacity);
    if (ok) *sim = *candidate;
    free(candidate);
    return ok;
}

bool CcCoopAdvance(CcSim *sim, int32_t ticks, char *error, size_t capacity)
{
    if (sim == NULL || ticks < 0 || ticks > 3600 ||
        sim->clock.tick > UINT64_MAX - (uint64_t)ticks) return false;
    CcSim *candidate = malloc(sizeof(*candidate));
    if (candidate == NULL) return false;
    *candidate = *sim;
    /* Watches flow into a short break or overnight camp automatically. */
    for (int32_t tick = 0; tick < ticks; ++tick) {
        if (candidate->journey.active && candidate->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {.kind = CcSimJourneyStop(candidate) == CC_JOURNEY_STOP_MIDDAY ?
                CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP};
            if (!CcSimApply(candidate, &rest, error, capacity)) { free(candidate); return false; }
        }
        CcSimAdvanceRuntimeTicks(candidate, 1);
        if (!candidate->journey.active ||
            candidate->journey.phase == CC_JOURNEY_PHASE_BLOCKED) break;
    }
    bool ok = CcSimValidate(candidate, error, capacity);
    if (ok) *sim = *candidate;
    free(candidate);
    return ok;
}

typedef struct Json {
    char *text;
    size_t capacity;
    size_t used;
    bool ok;
} Json;

static void Put(Json *json, const char *format, ...)
{
    if (!json->ok) return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(json->text + json->used, json->capacity - json->used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= json->capacity - json->used) {
        json->ok = false;
        return;
    }
    json->used += (size_t)written;
}

static void Quote(Json *json, const char *text)
{
    Put(json, "\"");
    for (const unsigned char *c = (const unsigned char *)text; *c != 0U; ++c) {
        if (*c == '"' || *c == '\\') Put(json, "\\%c", *c);
        else if (*c < 32U) Put(json, "\\u%04x", (unsigned int)*c);
        else Put(json, "%c", *c);
    }
    Put(json, "\"");
}

static void Goods(Json *json, const int32_t *goods)
{
    Put(json, "[");
    for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) Put(json, "%s%d", i ? "," : "", goods[i]);
    Put(json, "]");
}

bool CcCoopSnapshot(const CcSim *sim, char *text, size_t capacity)
{
    if (sim == NULL || text == NULL || capacity == 0U) return false;
    Json json = {text, capacity, 0U, true};
    Put(&json, "{\"protocol\":%d,\"hash\":\"%016" PRIx64 "\",\"seed\":%u,\"day\":%d,\"tick\":%" PRIu64 ",\"minute\":%d,",
        CC_COOP_PROTOCOL_VERSION, CcSimHash(sim), sim->world_seed, sim->current_day,
        sim->clock.tick, sim->clock.minute_subticks / CC_WORLD_MINUTE_SUBTICKS);
    Put(&json, "\"company\":{\"id\":\"%" PRIu64 "\",\"location\":\"%" PRIu64 "\",\"coins\":%" PRId64 ",\"reputation\":%d,\"capacity\":%d,\"cargo_used\":%d,\"accepted\":\"%" PRIu64 "\",\"cargo\":",
        sim->player.id, sim->player.location_id, sim->player.coins, sim->player.reputation,
        sim->player.cargo_capacity, CcPlayerCargoUsed(&sim->player), sim->player.accepted_situation_id);
    Goods(&json, sim->player.cargo);
    Put(&json, "},\"journey\":{\"active\":%s,\"phase\":%d,\"route\":\"%" PRIu64 "\",\"origin\":\"%" PRIu64 "\",\"destination\":\"%" PRIu64 "\",\"pace\":%d,\"progress\":%d,\"watch\":%d,\"watches\":%d,\"eta\":%d,\"stop\":%d,\"lodge\":%s,\"bargain\":%d,\"road_site\":",
        sim->journey.active ? "true" : "false", (int)sim->journey.phase,
        sim->journey.route_id, sim->journey.origin_id, sim->journey.destination_id,
        (int)sim->journey.pace, sim->carriage.progress_milli,
        CcSimJourneyWatchNumber(sim), CcSimJourneyWatchCount(sim), CcSimJourneyEtaMinutes(sim),
        (int)CcSimJourneyStop(sim), CcSimJourneyRoadHouseAvailable(sim) ? "true" : "false",
        sim->journey.bargain_cost);
    const CcRoadSite *road_site = CcSimJourneyRoadSiteStop(sim);
    if (road_site != NULL) {
        Put(&json, "{\"id\":\"%" PRIu64 "\",\"name\":", road_site->id);
        Quote(&json, road_site->name);
        Put(&json, "}");
    } else Put(&json, "null");
    Put(&json, "},");
    Put(&json, "\"goods\":[");
    for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) {
        if (i) Put(&json, ",");
        Quote(&json, CcGoodName((CcGood)i));
    }
    Put(&json, "],\"settlements\":[");
    bool comma = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (!CcSimPlayerKnowsSettlement(sim, place->id)) continue;
        Put(&json, "%s{\"id\":\"%" PRIu64 "\",\"name\":", comma ? "," : "", place->id);
        comma = true;
        Quote(&json, place->name);
        Put(&json, ",\"x\":%d,\"y\":%d}", place->map_x, place->map_y);
    }
    Put(&json, "],\"market\":");
    const CcSettlement *market = CcSimSettlement(sim, sim->player.location_id);
    if (market != NULL && !sim->journey.active) {
        Put(&json, "{\"name\":"); Quote(&json, market->name);
        Put(&json, ",\"hunger\":%d,\"stock\":", market->hunger); Goods(&json, market->stock);
        Put(&json, ",\"prices\":"); Goods(&json, market->price); Put(&json, "}");
    } else Put(&json, "null");
    Put(&json, ",\"roads\":[");
    comma = false;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        int32_t from = 0, to = 0;
        bool charted = false;
        if (!CcSimPlayerRouteReveal(sim, route->id, &from, &to, &charted)) continue;
        Put(&json, "%s{\"id\":\"%" PRIu64 "\",\"from\":\"%" PRIu64 "\",\"to\":\"%" PRIu64 "\",\"closed\":%s}",
            comma ? "," : "", route->id, route->from_id, route->to_id, route->closed ? "true" : "false");
        comma = true;
    }
    Put(&json, "],\"travel\":[");
    comma = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->id == sim->player.location_id || !CcSimPlayerKnowsSettlement(sim, place->id)) continue;
        const CcRoute *route = CcSimRouteBetween(sim, sim->player.location_id, place->id);
        if (route == NULL) continue;
        CcTravelPreview preview = {0};
        char reason[192] = "";
        bool can = !sim->journey.active && CcSimTravelPreview(sim, place->id, &preview, reason, sizeof(reason));
        Put(&json, "%s{\"id\":\"%" PRIu64 "\",\"route\":\"%" PRIu64 "\",\"name\":", comma ? "," : "", place->id, route->id);
        comma = true; Quote(&json, place->name);
        Put(&json, ",\"available\":%s,\"closed\":%s,\"fare\":%" PRId64 ",\"watches\":%d,\"reason\":",
            can ? "true" : "false", route->closed ? "true" : "false", preview.provision_cost, preview.travel_watches);
        Quote(&json, reason); Put(&json, "}");
    }
    Put(&json, "],\"charters\":[");
    comma = false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *s = &sim->situations[i];
        bool accepted = sim->player.accepted_situation_id == s->id;
        if (!accepted && !CcSimSituationCanAccept(sim, s)) continue;
        Put(&json, "%s{\"id\":\"%" PRIu64 "\",\"kind\":", comma ? "," : "", s->id);
        comma = true; Quote(&json, CcSituationKindName(s->kind));
        Put(&json, ",\"sponsor\":"); Quote(&json, s->sponsor_name);
        Put(&json, ",\"good\":%d,\"quantity\":%d,\"progress\":%d,\"reward\":%" PRId64 ",\"accepted\":%s}",
            (int)s->good, s->quantity, s->progress, (int64_t)s->reward, accepted ? "true" : "false");
    }
    Put(&json, "],\"events\":[");
    comma = false;
    for (int32_t i = 0; i < sim->event_count && i < 24; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) continue;
        if (event->subject_id != sim->player.id && event->actor_id != sim->player.id &&
            event->location_id != sim->player.location_id &&
            (!sim->journey.active || event->location_id != sim->journey.route_id)) continue;
        Put(&json, "%s{\"id\":\"%" PRIu64 "\",\"day\":%d,\"text\":", comma ? "," : "", event->id, event->day);
        comma = true; Quote(&json, event->text); Put(&json, "}");
    }
    Put(&json, "]}");
    return json.ok;
}
