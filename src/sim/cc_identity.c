#include "sim/cc_identity_internal.h"

#include <stdio.h>

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

#define CC_MAX_TRACKED_IDENTITIES \
    (CC_MAX_KINGDOMS + CC_MAX_SETTLEMENTS + CC_MAX_ROUTES + CC_MAX_MAPS + \
     CC_MAX_ROAD_SITES + CC_MAX_TREASURES + CC_MAX_FACTIONS + CC_MAX_SHIPMENTS + \
     CC_MAX_COURIERS + CC_MAX_BANDITS + CC_MAX_MONSTERS + CC_MAX_DUNGEONS + \
     CC_MAX_SITUATIONS + CC_MAX_FRONTS + CC_MAX_QUEST_OUTCOMES + \
     CC_MAX_CHARACTERS + CC_MAX_EVENTS + \
     CC_CARRIAGE_HORSE_COUNT + \
     CC_MAX_STABLE_HORSES + CC_MAX_KINGDOMS + 4)

typedef struct CcIdentityLedger {
    CcId ids[CC_MAX_TRACKED_IDENTITIES];
    int32_t count;
    uint64_t greatest_serial;
} CcIdentityLedger;

static bool TrackIdentity(CcIdentityLedger *ledger, CcId id,
                          CcEntityKind expected_kind,
                          char *error, size_t error_capacity)
{
    uint64_t serial = id & CC_ID_SERIAL_MASK;
    if (CcIdKind(id) != expected_kind || serial == 0U ||
        ledger->count >= CC_MAX_TRACKED_IDENTITIES) {
        SetError(error, error_capacity, "Simulation identity is invalid.");
        return false;
    }
    for (int32_t i = 0; i < ledger->count; ++i) {
        if (ledger->ids[i] == id) {
            SetError(error, error_capacity,
                     "Simulation identities are not unique.");
            return false;
        }
    }
    ledger->ids[ledger->count++] = id;
    if (serial > ledger->greatest_serial) ledger->greatest_serial = serial;
    return true;
}

bool CcIdentityValidate(const CcSim *sim,
                                  char *error, size_t error_capacity)
{
    CcIdentityLedger ledger = {0};
#define TRACK_ID(value, kind) \
    do { \
        if (!TrackIdentity(&ledger, (value), (kind), \
                           error, error_capacity)) return false; \
    } while (0)
    for (int32_t i = 0; i < sim->kingdom_count; ++i)
        TRACK_ID(sim->kingdoms[i].id, CC_ENTITY_KINGDOM);
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        TRACK_ID(sim->settlements[i].id, CC_ENTITY_SETTLEMENT);
    for (int32_t i = 0; i < sim->route_count; ++i)
        TRACK_ID(sim->routes[i].id, CC_ENTITY_ROUTE);
    if (sim->schema_version >= 31U) {
        for (int32_t i = 0; i < sim->road_site_count; ++i)
            TRACK_ID(sim->road_sites[i].id, CC_ENTITY_ROAD_SITE);
    }
    for (int32_t i = 0; i < sim->map_count; ++i)
        TRACK_ID(sim->maps[i].id, CC_ENTITY_MAP);
    if (sim->schema_version >= 9U) {
        for (int32_t i = 0; i < sim->treasure_count; ++i)
            TRACK_ID(sim->treasures[i].id, CC_ENTITY_TREASURE);
    }
    for (int32_t i = 0; i < sim->faction_count; ++i)
        TRACK_ID(sim->factions[i].id, CC_ENTITY_FACTION);
    for (int32_t i = 0; i < sim->shipment_count; ++i)
        TRACK_ID(sim->shipments[i].id, CC_ENTITY_SHIPMENT);
    if (sim->schema_version >= 38U) {
        if (sim->royal_trade_week != sim->current_day / 7) {
            SetError(error, error_capacity,
                     "Royal route usage week is invalid.");
            return false;
        }
        for (int32_t route = 0; route < sim->route_count; ++route) {
            if (sim->royal_route_slots_used[route] < 0 ||
                sim->royal_route_slots_used[route] > CC_SIM_MAX_UNITS) {
                SetError(error, error_capacity,
                         "Royal route usage is invalid.");
                return false;
            }
        }
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            TRACK_ID(sim->royal_carriages[i].id,
                     CC_ENTITY_ROYAL_CARRIAGE);
        }
    }
    if (sim->schema_version >= 11U) {
        for (int32_t i = 0; i < sim->courier_count; ++i)
            TRACK_ID(sim->couriers[i].id, CC_ENTITY_COURIER);
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i)
        TRACK_ID(sim->bandits[i].id, CC_ENTITY_BANDIT_GROUP);
    for (int32_t i = 0; i < sim->monster_count; ++i)
        TRACK_ID(sim->monsters[i].id, CC_ENTITY_MONSTER_POPULATION);
    for (int32_t i = 0; i < sim->dungeon_count; ++i)
        TRACK_ID(sim->dungeons[i].id, CC_ENTITY_DUNGEON);
    for (int32_t i = 0; i < sim->situation_count; ++i)
        TRACK_ID(sim->situations[i].id, CC_ENTITY_SITUATION);
    if (sim->schema_version >= 19U) {
        for (int32_t i = 0; i < sim->front_count; ++i)
            TRACK_ID(sim->fronts[i].id, CC_ENTITY_FRONT);
        for (int32_t i = 0; i < sim->quest_outcome_count; ++i)
            TRACK_ID(sim->quest_outcomes[i].id, CC_ENTITY_QUEST_OUTCOME);
    }
    if (sim->schema_version >= 17U) {
        for (int32_t i = 0; i < sim->character_count; ++i)
            TRACK_ID(sim->characters[i].id, CC_ENTITY_CHARACTER);
    }
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) {
            SetError(error, error_capacity, "Simulation event identity is invalid.");
            return false;
        }
        TRACK_ID(event->id, CC_ENTITY_EVENT);
    }
    TRACK_ID(sim->player.id, CC_ENTITY_PLAYER_COMPANY);
    if (sim->schema_version >= 6U) {
        TRACK_ID(sim->goblins.id, CC_ENTITY_GOBLIN_CULT);
        TRACK_ID(sim->dragon.id, CC_ENTITY_DRAGON);
    }
    if (sim->schema_version >= 7U)
        TRACK_ID(sim->hoard_raiders.id, CC_ENTITY_HOARD_RAIDERS);
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i)
            TRACK_ID(sim->horse_team[i].id, CC_ENTITY_HORSE);
    }
    if (sim->schema_version >= 15U) {
        for (int32_t i = 0; i < sim->stable_horse_count; ++i)
            TRACK_ID(sim->stable_horses[i].id, CC_ENTITY_HORSE);
    }
#undef TRACK_ID
    if (sim->next_entity_serial <= ledger.greatest_serial ||
        sim->next_entity_serial > CC_ID_SERIAL_MASK) {
        SetError(error, error_capacity,
                 "Simulation identity counter is behind saved entities.");
        return false;
    }
    return true;
}

