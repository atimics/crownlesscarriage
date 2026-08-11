#include "sim/cc_sim.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CC_ID_SERIAL_MASK UINT64_C(0x00ffffffffffffff)

static void GenerateSituations(CcSim *sim);

static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t MinimumI32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static int32_t MaximumI32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static int32_t AbsoluteI32(int32_t value)
{
    return value < 0 ? -value : value;
}

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

static uint32_t NextRandom(CcSim *sim)
{
    uint32_t value = sim->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    sim->random_state = value == 0U ? UINT32_C(0xa341316c) : value;
    return sim->random_state;
}

static CcId NextId(CcSim *sim, CcEntityKind kind)
{
    CcId id = CcMakeId(kind, sim->next_entity_serial);
    sim->next_entity_serial += 1U;
    return id;
}

CcId CcMakeId(CcEntityKind kind, uint64_t serial)
{
    return ((uint64_t)kind << 56U) | (serial & CC_ID_SERIAL_MASK);
}

CcEntityKind CcIdKind(CcId id)
{
    return (CcEntityKind)((id >> 56U) & UINT64_C(0xff));
}

static void CopyName(char destination[CC_NAME_CAPACITY], const char *name)
{
    (void)snprintf(destination, CC_NAME_CAPACITY, "%s", name);
}

static void GeneratePlaceName(CcSim *sim, char destination[CC_NAME_CAPACITY],
                              CcSettlementFunction function)
{
    static const char *prefixes[] = {
        "Rose", "Mere", "Iron", "Hollow", "Star", "Alder", "Ash",
        "Glass", "Willow", "Thorn", "Silver", "Moss", "Ember", "Gloam"
    };
    static const char *suffixes[][4] = {
        {"fall", "field", "mead", "acre"},
        {"cross", "market", "gate", "exchange"},
        {"watch", "bridge", "ward", "keep"},
        {"wick", "delve", "forge", "pit"},
        {"haven", "court", "spire", "crown"},
        {"ford", "deep", "barrow", "hollow"}
    };
    uint32_t prefix_count = (uint32_t)(sizeof(prefixes) / sizeof(prefixes[0]));
    uint32_t function_index = (uint32_t)function;
    if (function_index >= 6U) function_index = 0U;
    const char *prefix = prefixes[NextRandom(sim) % prefix_count];
    const char *suffix = suffixes[function_index][NextRandom(sim) % 4U];
    (void)snprintf(destination, CC_NAME_CAPACITY, "%s%s", prefix, suffix);
}

static int32_t Jitter(CcSim *sim, int32_t center, int32_t radius)
{
    uint32_t span = (uint32_t)(radius * 2 + 1);
    return center + (int32_t)(NextRandom(sim) % span) - radius;
}

static CcEvent *PushEvent(CcSim *sim, CcEventKind kind, CcId subject,
                          CcId location, CcId parent, int32_t magnitude,
                          const char *text)
{
    int32_t slot = sim->event_write_index;
    CcEvent *event = &sim->events[slot];
    *event = (CcEvent){0};
    event->id = NextId(sim, CC_ENTITY_EVENT);
    event->day = sim->current_day;
    event->kind = kind;
    event->subject_id = subject;
    event->location_id = location;
    event->parent_id = parent;
    event->magnitude = magnitude;
    (void)snprintf(event->text, sizeof(event->text), "%s", text);
    sim->event_write_index = (slot + 1) % CC_MAX_EVENTS;
    if (sim->event_count < CC_MAX_EVENTS) sim->event_count += 1;
    return event;
}

const char *CcGoodName(CcGood good)
{
    switch (good) {
        case CC_GOOD_FOOD: return "Food";
        case CC_GOOD_MATERIAL: return "Material";
        case CC_GOOD_TOOLS: return "Tools";
        case CC_GOOD_COUNT: break;
    }
    return "Unknown";
}

const char *CcSettlementFunctionName(CcSettlementFunction function)
{
    switch (function) {
        case CC_SETTLEMENT_FARMING: return "Agricultural basin";
        case CC_SETTLEMENT_MINING: return "Mining town";
        case CC_SETTLEMENT_MARKET: return "Crossroads market";
        case CC_SETTLEMENT_FORTRESS: return "Bridge fortress";
        case CC_SETTLEMENT_CAPITAL: return "Administrative capital";
        case CC_SETTLEMENT_DUNGEON_TOWN: return "Dungeon frontier";
    }
    return "Settlement";
}

const char *CcDungeonStateName(CcDungeonState state)
{
    switch (state) {
        case CC_DUNGEON_SEALED: return "Sealed";
        case CC_DUNGEON_DISTURBED: return "Disturbed";
        case CC_DUNGEON_EXPLORED: return "Explored";
        case CC_DUNGEON_PUBLIC_ROUTE: return "Public route";
        case CC_DUNGEON_SMUGGLER_ROUTE: return "Smuggler route";
        case CC_DUNGEON_RESEALED: return "Resealed";
    }
    return "Unknown";
}

const char *CcEventKindName(CcEventKind kind)
{
    switch (kind) {
        case CC_EVENT_HARVEST_FAILED: return "HARVEST";
        case CC_EVENT_ROUTE_CLOSED: return "ROUTE";
        case CC_EVENT_SHORTAGE: return "SHORTAGE";
        case CC_EVENT_SHIPMENT_DEPARTED: return "SHIPMENT";
        case CC_EVENT_SHIPMENT_ARRIVED: return "ARRIVAL";
        case CC_EVENT_BANDIT_PRESSURE: return "BANDITS";
        case CC_EVENT_MONSTER_PRESSURE: return "MONSTERS";
        case CC_EVENT_PLAYER_TRADE: return "TRADE";
        case CC_EVENT_PLAYER_TRAVEL: return "TRAVEL";
        case CC_EVENT_ROUTE_REPAIRED: return "REPAIRED";
        case CC_EVENT_DUNGEON_CHANGED: return "DUNGEON";
        case CC_EVENT_RELIEF: return "RELIEF";
        case CC_EVENT_SHIPMENT_LOST: return "AMBUSH";
        case CC_EVENT_KINGDOM_ACTION: return "DECREE";
        case CC_EVENT_ROUTE_DECAY: return "ROAD";
        case CC_EVENT_FACTION_SHIFT: return "POLITICS";
        case CC_EVENT_SITUATION_CREATED: return "CONTRACT";
        case CC_EVENT_SITUATION_RESOLVED: return "FULFILLED";
        case CC_EVENT_SITUATION_FAILED: return "FAILED";
        case CC_EVENT_PLAYER_AMBUSH: return "AMBUSH";
    }
    return "EVENT";
}

const char *CcSituationKindName(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY: return "Relief charter";
        case CC_SITUATION_ROUTE_REPAIR: return "Road compact";
        case CC_SITUATION_MONSTER_EXPEDITION: return "Depth warrant";
        case CC_SITUATION_BLACK_MARKET_DELIVERY: return "Quiet commission";
    }
    return "Unknown situation";
}

const CcSettlement *CcSimSettlement(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_SETTLEMENT) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id == id) return &sim->settlements[i];
    }
    return NULL;
}

CcSettlement *CcSimSettlementMutable(CcSim *sim, CcId id)
{
    return (CcSettlement *)CcSimSettlement((const CcSim *)sim, id);
}

const CcRoute *CcSimRoute(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_ROUTE) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].id == id) return &sim->routes[i];
    }
    return NULL;
}

static CcRoute *RouteMutable(CcSim *sim, CcId id)
{
    return (CcRoute *)CcSimRoute((const CcSim *)sim, id);
}

const CcRoute *CcSimRouteBetween(const CcSim *sim, CcId a, CcId b)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if ((route->from_id == a && route->to_id == b) ||
            (route->from_id == b && route->to_id == a)) return route;
    }
    return NULL;
}

const CcEvent *CcSimRecentEvent(const CcSim *sim, int32_t offset)
{
    if (sim == NULL || offset < 0 || offset >= sim->event_count) return NULL;
    int32_t slot = sim->event_write_index - 1 - offset;
    while (slot < 0) slot += CC_MAX_EVENTS;
    return &sim->events[slot];
}

static const CcEvent *LatestEvent(const CcSim *sim, CcEventKind kind,
                                  CcId subject, CcId location)
{
    if (sim == NULL) return NULL;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->kind != kind) continue;
        if (subject != 0U && event->subject_id != subject) continue;
        if (location != 0U && event->location_id != location) continue;
        return event;
    }
    return NULL;
}

static CcId LatestLocalCause(const CcSim *sim, CcId location)
{
    if (sim == NULL) return 0U;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->location_id != location) continue;
        if (event->kind == CC_EVENT_PLAYER_TRADE ||
            event->kind == CC_EVENT_PLAYER_TRAVEL ||
            event->kind == CC_EVENT_SITUATION_CREATED) continue;
        return event->id;
    }
    return 0U;
}

int32_t CcSimActiveSituationCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) count += 1;
    }
    return count;
}

static bool SituationTouchesSettlement(const CcSim *sim,
                                       const CcSituation *situation,
                                       CcId settlement_id)
{
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        return situation->target_id == settlement_id;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        return route != NULL &&
               (route->from_id == settlement_id || route->to_id == settlement_id);
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].id == situation->target_id) {
                return sim->dungeons[i].settlement_id == settlement_id;
            }
        }
    }
    return false;
}

const CcSituation *CcSimSituationForSettlement(const CcSim *sim,
                                                CcId settlement_id)
{
    if (sim == NULL) return NULL;
    const CcSituation *best = NULL;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            !SituationTouchesSettlement(sim, situation, settlement_id)) continue;
        if (best == NULL || situation->deadline_day < best->deadline_day) best = situation;
    }
    return best;
}

int32_t CcSimIncomingGood(const CcSim *sim, CcId settlement_id, CcGood good)
{
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    int32_t incoming = 0;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        if (shipment->status == CC_SHIPMENT_TRAVELLING &&
            shipment->final_destination_id == settlement_id && shipment->good == good) {
            incoming += shipment->quantity;
        }
    }
    return incoming;
}

int32_t CcPlayerCargoUsed(const CcPlayerCompany *player)
{
    if (player == NULL) return 0;
    int32_t used = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        used += player->cargo[good];
    }
    return used;
}

static void InitKingdom(CcSim *sim, int32_t slot, const char *name,
                        uint8_t red, uint8_t green, uint8_t blue,
                        CcMoney treasury, int32_t legitimacy)
{
    CcKingdom *kingdom = &sim->kingdoms[slot];
    kingdom->id = NextId(sim, CC_ENTITY_KINGDOM);
    CopyName(kingdom->name, name);
    kingdom->color_r = red;
    kingdom->color_g = green;
    kingdom->color_b = blue;
    kingdom->treasury = treasury;
    kingdom->legitimacy = legitimacy;
}

static void InitSettlement(CcSim *sim, int32_t slot, int32_t kingdom_slot,
                           const char *name, CcSettlementFunction function,
                           int32_t x, int32_t y, int32_t population,
                           int32_t security, int32_t prosperity)
{
    CcSettlement *settlement = &sim->settlements[slot];
    settlement->id = NextId(sim, CC_ENTITY_SETTLEMENT);
    settlement->kingdom_id = sim->kingdoms[kingdom_slot].id;
    CopyName(settlement->name, name);
    settlement->function = function;
    settlement->map_x = x;
    settlement->map_y = y;
    settlement->population = population;
    settlement->security = security;
    settlement->prosperity = prosperity;
    settlement->hunger = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        settlement->price[good] = good == CC_GOOD_FOOD ? 4 :
                                  good == CC_GOOD_MATERIAL ? 8 : 14;
    }
}

static void InitRoute(CcSim *sim, int32_t slot, int32_t from, int32_t to,
                      int32_t days, int32_t capacity, int32_t security,
                      bool closed, bool smuggler)
{
    CcRoute *route = &sim->routes[slot];
    route->id = NextId(sim, CC_ENTITY_ROUTE);
    route->from_id = sim->settlements[from].id;
    route->to_id = sim->settlements[to].id;
    route->travel_days = days;
    route->capacity = capacity;
    route->security = security;
    route->condition = closed ? 38 : 76;
    route->closed = closed;
    route->smuggler_route = smuggler;
}

static void InitFaction(CcSim *sim, int32_t slot, int32_t kingdom_slot,
                        CcFactionKind kind, const char *name,
                        int32_t power, int32_t support)
{
    CcFaction *faction = &sim->factions[slot];
    faction->id = NextId(sim, CC_ENTITY_FACTION);
    faction->kingdom_id = sim->kingdoms[kingdom_slot].id;
    faction->kind = kind;
    CopyName(faction->name, name);
    faction->power = power;
    faction->support = support;
}

static void ConfigureSettlementEconomies(CcSim *sim)
{
    CcSettlement *farm = &sim->settlements[0];
    farm->stock[CC_GOOD_FOOD] = 74;
    farm->stock[CC_GOOD_MATERIAL] = 12;
    farm->stock[CC_GOOD_TOOLS] = 8;
    farm->reserve_target[CC_GOOD_FOOD] = 105;
    farm->reserve_target[CC_GOOD_MATERIAL] = 18;
    farm->reserve_target[CC_GOOD_TOOLS] = 18;
    farm->production[CC_GOOD_FOOD] = 30;
    farm->production[CC_GOOD_MATERIAL] = 1;
    farm->consumption[CC_GOOD_FOOD] = 5;
    farm->consumption[CC_GOOD_TOOLS] = 1;

    CcSettlement *market = &sim->settlements[1];
    market->stock[CC_GOOD_FOOD] = 35;
    market->stock[CC_GOOD_MATERIAL] = 32;
    market->stock[CC_GOOD_TOOLS] = 23;
    market->reserve_target[CC_GOOD_FOOD] = 75;
    market->reserve_target[CC_GOOD_MATERIAL] = 45;
    market->reserve_target[CC_GOOD_TOOLS] = 35;
    market->production[CC_GOOD_TOOLS] = 4;
    market->consumption[CC_GOOD_FOOD] = 7;
    market->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *fortress = &sim->settlements[2];
    fortress->stock[CC_GOOD_FOOD] = 42;
    fortress->stock[CC_GOOD_MATERIAL] = 18;
    fortress->stock[CC_GOOD_TOOLS] = 15;
    fortress->reserve_target[CC_GOOD_FOOD] = 70;
    fortress->reserve_target[CC_GOOD_MATERIAL] = 28;
    fortress->reserve_target[CC_GOOD_TOOLS] = 24;
    fortress->consumption[CC_GOOD_FOOD] = 6;
    fortress->consumption[CC_GOOD_TOOLS] = 1;

    CcSettlement *mine = &sim->settlements[3];
    mine->stock[CC_GOOD_FOOD] = 21;
    mine->stock[CC_GOOD_MATERIAL] = 76;
    mine->stock[CC_GOOD_TOOLS] = 13;
    mine->reserve_target[CC_GOOD_FOOD] = 78;
    mine->reserve_target[CC_GOOD_MATERIAL] = 55;
    mine->reserve_target[CC_GOOD_TOOLS] = 38;
    mine->production[CC_GOOD_MATERIAL] = 10;
    mine->consumption[CC_GOOD_FOOD] = 7;
    mine->consumption[CC_GOOD_TOOLS] = 2;

    CcSettlement *capital = &sim->settlements[4];
    capital->stock[CC_GOOD_FOOD] = 61;
    capital->stock[CC_GOOD_MATERIAL] = 36;
    capital->stock[CC_GOOD_TOOLS] = 31;
    capital->reserve_target[CC_GOOD_FOOD] = 92;
    capital->reserve_target[CC_GOOD_MATERIAL] = 48;
    capital->reserve_target[CC_GOOD_TOOLS] = 42;
    capital->production[CC_GOOD_FOOD] = 10;
    capital->production[CC_GOOD_TOOLS] = 5;
    capital->consumption[CC_GOOD_FOOD] = 8;
    capital->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *frontier = &sim->settlements[5];
    frontier->stock[CC_GOOD_FOOD] = 39;
    frontier->stock[CC_GOOD_MATERIAL] = 48;
    frontier->stock[CC_GOOD_TOOLS] = 17;
    frontier->reserve_target[CC_GOOD_FOOD] = 62;
    frontier->reserve_target[CC_GOOD_MATERIAL] = 44;
    frontier->reserve_target[CC_GOOD_TOOLS] = 28;
    frontier->production[CC_GOOD_FOOD] = 3;
    frontier->production[CC_GOOD_MATERIAL] = 4;
    frontier->consumption[CC_GOOD_FOOD] = 5;
    frontier->consumption[CC_GOOD_TOOLS] = 1;

    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        settlement->population += (int32_t)(NextRandom(sim) % 401U) - 200;
        settlement->security = ClampI32(settlement->security +
            (int32_t)(NextRandom(sim) % 15U) - 7, 20, 92);
        settlement->prosperity = ClampI32(settlement->prosperity +
            (int32_t)(NextRandom(sim) % 15U) - 7, 20, 88);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            settlement->stock[good] = MaximumI32(1, settlement->stock[good] +
                (int32_t)(NextRandom(sim) % 13U) - 6);
            settlement->production[good] = MaximumI32(0, settlement->production[good] +
                (settlement->production[good] > 0 ? (int32_t)(NextRandom(sim) % 3U) - 1 : 0));
        }
    }
}

void CcSimInit(CcSim *sim, uint32_t seed)
{
    if (sim == NULL) return;
    *sim = (CcSim){0};
    sim->schema_version = CC_SIM_SCHEMA_VERSION;
    sim->generator_version = CC_GENERATOR_VERSION;
    sim->world_seed = seed == 0U ? UINT32_C(0xc0a71a9e) : seed;
    sim->random_state = sim->world_seed;
    sim->current_day = 1;
    sim->next_entity_serial = 1U;

    static const char *kingdom_sets[][3] = {
        {"Alder Concord", "Ember Crown", "Star Oath"},
        {"Willow Republic", "Ashen Throne", "Indigo March"},
        {"Moss Compact", "Rose Dominion", "Silver Covenant"}
    };
    uint32_t name_set = NextRandom(sim) % 3U;
    InitKingdom(sim, 0, kingdom_sets[name_set][0], 54U, 173U, 146U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                55 + (int32_t)(NextRandom(sim) % 17U));
    InitKingdom(sim, 1, kingdom_sets[name_set][1], 210U, 101U, 71U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                48 + (int32_t)(NextRandom(sim) % 17U));
    InitKingdom(sim, 2, kingdom_sets[name_set][2], 102U, 123U, 205U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                58 + (int32_t)(NextRandom(sim) % 17U));
    sim->kingdom_count = CC_MAX_KINGDOMS;

    char place_names[CC_MAX_SETTLEMENTS][CC_NAME_CAPACITY];
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        GeneratePlaceName(sim, place_names[i], (CcSettlementFunction)i);
    }
    InitSettlement(sim, 0, 0, place_names[0], CC_SETTLEMENT_FARMING,
                   Jitter(sim, 125, 22), Jitter(sim, 500, 20), 1460, 58, 54);
    InitSettlement(sim, 1, 0, place_names[2], CC_SETTLEMENT_MARKET,
                   Jitter(sim, 355, 22), Jitter(sim, 445, 20), 2180, 62, 67);
    InitSettlement(sim, 2, 1, place_names[3], CC_SETTLEMENT_FORTRESS,
                   Jitter(sim, 535, 18), Jitter(sim, 325, 18), 1720, 82, 49);
    InitSettlement(sim, 3, 1, place_names[1], CC_SETTLEMENT_MINING,
                   Jitter(sim, 755, 22), Jitter(sim, 455, 20), 2350, 43, 61);
    InitSettlement(sim, 4, 2, place_names[4], CC_SETTLEMENT_CAPITAL,
                   Jitter(sim, 770, 18), Jitter(sim, 145, 18), 3180, 71, 72);
    InitSettlement(sim, 5, 2, place_names[5], CC_SETTLEMENT_DUNGEON_TOWN,
                   Jitter(sim, 335, 22), Jitter(sim, 155, 20), 1280, 46, 45);
    sim->settlement_count = CC_MAX_SETTLEMENTS;
    ConfigureSettlementEconomies(sim);

    InitRoute(sim, 0, 0, 1, 2, 40, 64, false, false);
    InitRoute(sim, 1, 1, 2, 2, 34, 78, true, false);
    InitRoute(sim, 2, 2, 3, 2, 30, 34, false, false);
    InitRoute(sim, 3, 3, 4, 3, 28, 62, false, false);
    InitRoute(sim, 4, 4, 5, 3, 24, 45, false, false);
    InitRoute(sim, 5, 5, 1, 3, 24, 49, false, false);
    InitRoute(sim, 6, 1, 3, 4, 7, 20, false, true);
    InitRoute(sim, 7, 2, 4, 3, 26, 57, false, false);
    sim->route_count = CC_MAX_ROUTES;

    for (int32_t kingdom = 0; kingdom < CC_MAX_KINGDOMS; ++kingdom) {
        for (int32_t local = 0; local < 3; ++local) {
            int32_t slot = kingdom * 3 + local;
            char faction_name[CC_NAME_CAPACITY];
            const char *suffix = local == 0 ? " Court" :
                                 local == 1 ? " Factors" : " Commons";
            (void)snprintf(faction_name, sizeof(faction_name), "%.21s%s",
                           sim->kingdoms[kingdom].name, suffix);
            InitFaction(sim, slot, kingdom, (CcFactionKind)local,
                        faction_name,
                        54 + (int32_t)(NextRandom(sim) % 25U),
                        42 + (int32_t)(NextRandom(sim) % 31U));
        }
    }
    sim->faction_count = CC_MAX_FACTIONS;

    sim->bandits[0].id = NextId(sim, CC_ENTITY_BANDIT_GROUP);
    sim->bandits[0].route_id = sim->routes[6].id;
    static const char *bandit_names[] = {
        "The Unpaid Company", "The Tallow Knives", "The Broken Pennants",
        "The Ditch Parliament"
    };
    CopyName(sim->bandits[0].name, bandit_names[NextRandom(sim) % 4U]);
    sim->bandits[0].members = 28 + (int32_t)(NextRandom(sim) % 15U);
    sim->bandits[0].supplies = 20 + (int32_t)(NextRandom(sim) % 15U);
    sim->bandits[0].influence = 48 + (int32_t)(NextRandom(sim) % 16U);
    sim->bandit_count = 1;

    sim->dungeons[0].id = NextId(sim, CC_ENTITY_DUNGEON);
    sim->dungeons[0].settlement_id = sim->settlements[3].id;
    static const char *dungeon_names[] = {
        "The Lower Silverworks", "The Drowned Mint", "The Blackglass Galleries",
        "The Saintless Vault"
    };
    CopyName(sim->dungeons[0].name, dungeon_names[NextRandom(sim) % 4U]);
    sim->dungeons[0].state = CC_DUNGEON_DISTURBED;
    sim->dungeons[0].depth = 4;
    sim->dungeons[0].regional_pressure = 48;
    sim->dungeon_count = 1;

    sim->monsters[0].id = NextId(sim, CC_ENTITY_MONSTER_POPULATION);
    sim->monsters[0].dungeon_id = sim->dungeons[0].id;
    static const char *monster_names[] = {
        "Glassback Burrowers", "Ash-Maw Choir", "Pale Ore-Eaters",
        "Crownless Centipedes"
    };
    CopyName(sim->monsters[0].name, monster_names[NextRandom(sim) % 4U]);
    sim->monsters[0].population = 38 + (int32_t)(NextRandom(sim) % 17U);
    sim->monsters[0].pressure = 38 + (int32_t)(NextRandom(sim) % 15U);
    sim->monsters[0].hunting_pressure = 0;
    sim->monster_count = 1;

    sim->player.id = NextId(sim, CC_ENTITY_PLAYER_COMPANY);
    sim->player.location_id = sim->settlements[0].id;
    sim->player.coins = 42;
    sim->player.cargo_capacity = CC_CARGO_CAPACITY;
    sim->player.passenger_capacity = 4;
    sim->player.reputation = 0;

    char text[CC_EVENT_TEXT_CAPACITY];
    int32_t drought = 55 + (int32_t)(NextRandom(sim) % 26U);
    (void)snprintf(text, sizeof(text),
                   "%s's drought harvest cannot supply the eastern settlements.",
                   sim->settlements[0].name);
    CcEvent *harvest = PushEvent(sim, CC_EVENT_HARVEST_FAILED,
                                 sim->settlements[0].id,
                                 sim->settlements[0].id, 0, drought, text);
    (void)snprintf(text, sizeof(text),
                   "%s closes the treaty bridge and delays the relief convoy.",
                   sim->settlements[2].name);
    CcEvent *bridge = PushEvent(sim, CC_EVENT_ROUTE_CLOSED, sim->routes[1].id,
                                sim->settlements[2].id, harvest->id, 54, text);
    (void)snprintf(text, sizeof(text),
                   "Displaced workers reinforce %s on the old road.",
                   sim->bandits[0].name);
    (void)PushEvent(sim, CC_EVENT_BANDIT_PRESSURE, sim->bandits[0].id,
                    sim->routes[6].id, bridge->id,
                    sim->bandits[0].influence, text);
    (void)snprintf(text, sizeof(text),
                   "%s opens a sealed tunnel and disturbs the %s.",
                   sim->settlements[3].name, sim->monsters[0].name);
    (void)PushEvent(sim, CC_EVENT_MONSTER_PRESSURE, sim->monsters[0].id,
                    sim->dungeons[0].id, bridge->id,
                    sim->monsters[0].pressure, text);
    GenerateSituations(sim);
}

static int32_t BasePrice(CcGood good)
{
    return good == CC_GOOD_FOOD ? 4 : good == CC_GOOD_MATERIAL ? 8 : 14;
}

static CcDungeon *DungeonByIdMutable(CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == id) return &sim->dungeons[i];
    }
    return NULL;
}

static int32_t MonsterPressureAtSettlement(const CcSim *sim, CcId settlement_id)
{
    int32_t pressure = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id != settlement_id) continue;
        for (int32_t monster = 0; monster < sim->monster_count; ++monster) {
            if (sim->monsters[monster].dungeon_id == sim->dungeons[i].id) {
                pressure = MaximumI32(pressure, sim->monsters[monster].pressure);
            }
        }
    }
    return pressure;
}

static int32_t FoodSeasonFactor(const CcSim *sim)
{
    int32_t week = (sim->current_day / 7) % 52;
    if (week < 13) return 72;
    if (week < 26) return 112;
    if (week < 39) return 148;
    return 58;
}

static int32_t EffectiveProduction(CcSim *sim, CcSettlement *settlement,
                                   int32_t index, CcGood good)
{
    int32_t production = settlement->production[good];
    if (production <= 0) return 0;
    if (settlement->hunger > 65) production = production * 55 / 100;
    else if (settlement->hunger > 35) production = production * 78 / 100;

    if (good == CC_GOOD_FOOD) {
        production = production * FoodSeasonFactor(sim) / 100;
        if (index == 0 && sim->current_day < 112) production = production * 64 / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) production = production * 70 / 100;
    }
    if (good == CC_GOOD_MATERIAL) {
        int32_t monster_pressure = MonsterPressureAtSettlement(sim, settlement->id);
        production = production * (100 - monster_pressure / 2) / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) production = production * 58 / 100;
        else if (sim->current_day % 14 == 0) settlement->stock[CC_GOOD_TOOLS] -= 1;
        for (int32_t dungeon = 0; dungeon < sim->dungeon_count; ++dungeon) {
            if (sim->dungeons[dungeon].settlement_id == settlement->id &&
                sim->dungeons[dungeon].state == CC_DUNGEON_PUBLIC_ROUTE) {
                production = production * 125 / 100;
            }
        }
    }
    if (good == CC_GOOD_TOOLS) {
        int32_t material_input = MinimumI32(1, settlement->stock[CC_GOOD_MATERIAL]);
        settlement->stock[CC_GOOD_MATERIAL] -= material_input;
        production = production * (40 + material_input * 60) / 100;
    }
    return MaximumI32(0, production);
}

static void UpdateSettlement(CcSim *sim, int32_t index)
{
    CcSettlement *settlement = &sim->settlements[index];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t production = EffectiveProduction(sim, settlement, index, (CcGood)good);
        settlement->stock[good] += production;
        settlement->stock[good] -= MinimumI32(settlement->stock[good],
                                               settlement->consumption[good]);
        int32_t target = settlement->reserve_target[good];
        int32_t incoming = CcSimIncomingGood(sim, settlement->id, (CcGood)good);
        int32_t expected_stock = settlement->stock[good] + incoming / 2;
        int32_t shortage = target > 0 ? (target - expected_stock) * 100 / target : 0;
        int32_t pressure = ClampI32(shortage, -35, 220);
        settlement->price[good] = MinimumI32(99,
            BasePrice((CcGood)good) * (100 + pressure) / 100);
        if (settlement->price[good] < 1) settlement->price[good] = 1;
        settlement->stock[good] = MinimumI32(settlement->stock[good],
                                             target * 4 + 40);
    }

    int32_t food_use = MaximumI32(1, settlement->consumption[CC_GOOD_FOOD]);
    int32_t coverage = settlement->stock[CC_GOOD_FOOD] / food_use;
    int32_t hunger_delta = coverage == 0 ? 6 : coverage < 2 ? 3 :
                           coverage >= 8 ? -4 :
                           coverage >= 5 ? -2 : -1;
    settlement->hunger = ClampI32(settlement->hunger + hunger_delta, 0, 100);
    settlement->prosperity = ClampI32(settlement->prosperity +
        (settlement->hunger > 55 ? -2 : settlement->hunger > 30 ? -1 :
         coverage >= 8 ? 1 : 0), 0, 100);
    int32_t local_threat = MonsterPressureAtSettlement(sim, settlement->id);
    settlement->security = ClampI32(settlement->security +
        (settlement->hunger > 50 || local_threat > 60 ? -1 :
         settlement->prosperity > 70 && settlement->hunger < 15 ? 1 : 0), 0, 100);
    if (sim->current_day % 28 == 0) {
        int32_t population_delta = settlement->hunger > 65 ? -MaximumI32(1, settlement->population / 250) :
                                   settlement->prosperity > 70 && settlement->hunger < 15 ?
                                   MaximumI32(1, settlement->population / 500) : 0;
        settlement->population = MaximumI32(100, settlement->population + population_delta);
    }

    int32_t level = settlement->hunger >= 65 ? 3 :
                    settlement->hunger >= 40 ? 2 :
                    settlement->hunger >= 20 ? 1 : 0;
    if (level > sim->last_shortage_level[index]) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s has %d weeks of food; hunger reaches pressure level %d.",
                       settlement->name, coverage, level);
        (void)PushEvent(sim, CC_EVENT_SHORTAGE, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        settlement->hunger, text);
    }
    sim->last_shortage_level[index] = level;
}

static CcBanditGroup *BanditsOnRoute(CcSim *sim, CcId route_id)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == route_id) return &sim->bandits[i];
    }
    return NULL;
}

int32_t CcSimRouteDanger(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL || route->closed) return 100;
    int32_t danger = (100 - route->security) / 4 +
                     (100 - route->condition) / 8 +
                     (route->smuggler_route ? 5 : 0);
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == route_id) danger += sim->bandits[i].influence / 3;
    }
    danger += MonsterPressureAtSettlement(sim, route->from_id) / 10;
    danger += MonsterPressureAtSettlement(sim, route->to_id) / 10;
    return ClampI32(danger, 0, 95);
}

static int32_t SettlementSlotById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id == id) return i;
    }
    return -1;
}

static bool FindTradePath(const CcSim *sim, CcId from_id, CcId to_id,
                          int32_t *first_route_slot, CcId *first_hop_id,
                          int32_t *total_cost,
                          const int32_t route_used[CC_MAX_ROUTES])
{
    int32_t source = SettlementSlotById(sim, from_id);
    int32_t target = SettlementSlotById(sim, to_id);
    if (source < 0 || target < 0 || source == target) return false;
    int32_t distance[CC_MAX_SETTLEMENTS];
    int32_t first_route[CC_MAX_SETTLEMENTS];
    CcId first_hop[CC_MAX_SETTLEMENTS];
    bool visited[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        distance[i] = INT_MAX;
        first_route[i] = -1;
        first_hop[i] = 0U;
        visited[i] = false;
    }
    distance[source] = 0;
    for (int32_t iteration = 0; iteration < sim->settlement_count; ++iteration) {
        int32_t current = -1;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            if (!visited[i] && distance[i] < INT_MAX &&
                (current < 0 || distance[i] < distance[current])) current = i;
        }
        if (current < 0) break;
        if (current == target) break;
        visited[current] = true;
        CcId current_id = sim->settlements[current].id;
        for (int32_t route_slot = 0; route_slot < sim->route_count; ++route_slot) {
            const CcRoute *route = &sim->routes[route_slot];
            if (route->closed) continue;
            int32_t effective_capacity = MaximumI32(3, route->capacity *
                                                    MaximumI32(25, route->condition) / 100);
            if (route_used != NULL && route_used[route_slot] >= effective_capacity) continue;
            CcId neighbor_id = route->from_id == current_id ? route->to_id :
                               route->to_id == current_id ? route->from_id : 0U;
            int32_t neighbor = SettlementSlotById(sim, neighbor_id);
            if (neighbor < 0 || visited[neighbor]) continue;
            int32_t edge_cost = route->travel_days * 10 +
                                CcSimRouteDanger(sim, route->id);
            if (distance[current] > INT_MAX - edge_cost) continue;
            int32_t candidate = distance[current] + edge_cost;
            if (candidate >= distance[neighbor]) continue;
            distance[neighbor] = candidate;
            first_route[neighbor] = current == source ? route_slot : first_route[current];
            first_hop[neighbor] = current == source ? neighbor_id : first_hop[current];
        }
    }
    if (distance[target] == INT_MAX || first_route[target] < 0 || first_hop[target] == 0U) {
        return false;
    }
    if (first_route_slot != NULL) *first_route_slot = first_route[target];
    if (first_hop_id != NULL) *first_hop_id = first_hop[target];
    if (total_cost != NULL) *total_cost = distance[target];
    return true;
}

static void UpdateShipments(CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        CcShipment *shipment = &sim->shipments[i];
        if (shipment->status != CC_SHIPMENT_TRAVELLING ||
            shipment->arrival_day > sim->current_day) continue;
        CcId final_id = shipment->final_destination_id != 0U ?
                        shipment->final_destination_id : shipment->destination_id;
        CcBanditGroup *bandits = BanditsOnRoute(sim, shipment->route_id);
        int32_t danger = CcSimRouteDanger(sim, shipment->route_id);
        bool lost = (int32_t)(NextRandom(sim) % 100U) < danger;
        const CcEvent *departure = LatestEvent(sim, CC_EVENT_SHIPMENT_DEPARTED,
                                               shipment->id, 0U);
        if (lost) {
            shipment->status = CC_SHIPMENT_LOST;
            if (bandits != NULL) {
                bandits->supplies = ClampI32(bandits->supplies + shipment->quantity, 0, 100);
            }
            char text[CC_EVENT_TEXT_CAPACITY];
            const CcSettlement *destination = CcSimSettlement(sim, final_id);
            (void)snprintf(text, sizeof(text),
                           "%d %s bound for %s vanish on a road with %d%% danger.",
                           shipment->quantity, CcGoodName(shipment->good),
                           destination != NULL ? destination->name : "the frontier", danger);
            (void)PushEvent(sim, CC_EVENT_SHIPMENT_LOST, shipment->id,
                            final_id,
                            departure != NULL ? departure->id : 0U,
                            shipment->quantity, text);
            continue;
        }
        CcSettlement *hop = CcSimSettlementMutable(sim, shipment->destination_id);
        if (shipment->destination_id != final_id && hop != NULL) {
            int32_t local_need = hop->reserve_target[shipment->good] -
                                 hop->stock[shipment->good] -
                                 CcSimIncomingGood(sim, hop->id, shipment->good);
            int32_t unload = MinimumI32(local_need, shipment->quantity / 3);
            unload = MinimumI32(unload, shipment->quantity - 4);
            unload = MaximumI32(0, unload);
            if (unload > 0) {
                hop->stock[shipment->good] += unload;
                hop->prosperity = ClampI32(hop->prosperity + 1, 0, 100);
                shipment->quantity -= unload;
            }
            int32_t next_route_slot = -1;
            CcId next_hop_id = 0U;
            if (FindTradePath(sim, hop->id, final_id, &next_route_slot,
                              &next_hop_id, NULL, NULL)) {
                CcRoute *next_route = &sim->routes[next_route_slot];
                shipment->origin_id = hop->id;
                shipment->destination_id = next_hop_id;
                shipment->route_id = next_route->id;
                shipment->departure_day = sim->current_day;
                shipment->arrival_day = sim->current_day + next_route->travel_days;
                if (next_route->smuggler_route || sim->current_day % 21 == 0) {
                    next_route->condition = ClampI32(next_route->condition - 1, 0, 100);
                }
                const CcSettlement *final = CcSimSettlement(sim, final_id);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s unloads %d and transfers %d %s onward toward %s.",
                               hop->name, unload,
                               shipment->quantity, CcGoodName(shipment->good),
                               final != NULL ? final->name : "the frontier");
                (void)PushEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, shipment->id,
                                hop->id, departure != NULL ? departure->id : 0U,
                                shipment->quantity, text);
                continue;
            }
            hop->stock[shipment->good] += shipment->quantity;
            shipment->status = CC_SHIPMENT_ARRIVED;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%d %s become stranded at %s after their route changes.",
                           shipment->quantity, CcGoodName(shipment->good), hop->name);
            (void)PushEvent(sim, CC_EVENT_SHIPMENT_ARRIVED, shipment->id, hop->id,
                            departure != NULL ? departure->id : 0U,
                            shipment->quantity, text);
            continue;
        }
        CcSettlement *destination = CcSimSettlementMutable(sim, final_id);
        if (destination != NULL) {
            destination->stock[shipment->good] += shipment->quantity;
            destination->prosperity = ClampI32(destination->prosperity + 1, 0, 100);
        }
        shipment->status = CC_SHIPMENT_ARRIVED;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%d units of %s reach %s.",
                       shipment->quantity, CcGoodName(shipment->good),
                       destination != NULL ? destination->name : "their destination");
        (void)PushEvent(sim, CC_EVENT_SHIPMENT_ARRIVED, shipment->id,
                        final_id,
                        departure != NULL ? departure->id : 0U,
                        shipment->quantity, text);
    }
}

static CcShipment *AllocateShipment(CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status != CC_SHIPMENT_TRAVELLING) {
            sim->shipments[i] = (CcShipment){0};
            return &sim->shipments[i];
        }
    }
    if (sim->shipment_count >= CC_MAX_SHIPMENTS) return NULL;
    CcShipment *shipment = &sim->shipments[sim->shipment_count];
    sim->shipment_count += 1;
    return shipment;
}

static void CreateTradeShipment(CcSim *sim, int32_t route_slot, CcId next_hop_id,
                                CcGood good, CcSettlement *origin,
                                CcSettlement *final_destination,
                                int32_t route_used[CC_MAX_ROUTES])
{
    CcRoute *route = &sim->routes[route_slot];
    int32_t surplus = origin->stock[good] - origin->reserve_target[good];
    int32_t incoming = CcSimIncomingGood(sim, final_destination->id, good);
    int32_t need = final_destination->reserve_target[good] -
                   final_destination->stock[good] - incoming;
    int32_t effective_capacity = MaximumI32(3, route->capacity *
                                            MaximumI32(25, route->condition) / 100);
    int32_t available_capacity = effective_capacity - route_used[route_slot];
    int32_t quantity = MinimumI32(10, MinimumI32(available_capacity,
                                                 MinimumI32(surplus, need)));
    if (quantity < 4) return;
    CcShipment *shipment = AllocateShipment(sim);
    if (shipment == NULL) return;
    origin->stock[good] -= quantity;
    shipment->id = NextId(sim, CC_ENTITY_SHIPMENT);
    shipment->origin_id = origin->id;
    shipment->destination_id = next_hop_id;
    shipment->final_destination_id = final_destination->id;
    shipment->route_id = route->id;
    shipment->good = good;
    shipment->quantity = quantity;
    shipment->departure_day = sim->current_day;
    shipment->arrival_day = sim->current_day + route->travel_days;
    shipment->status = CC_SHIPMENT_TRAVELLING;
    route_used[route_slot] += quantity;
    if (route->smuggler_route || sim->current_day % 21 == 0) {
        route->condition = ClampI32(route->condition - 1, 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    const CcSettlement *next_hop = CcSimSettlement(sim, next_hop_id);
    (void)snprintf(text, sizeof(text), "%s sends %d %s toward %s via %s.",
                   origin->name, quantity, CcGoodName(good), final_destination->name,
                   next_hop != NULL ? next_hop->name : "the road");
    const CcEvent *need_event = LatestEvent(sim, CC_EVENT_SHORTAGE,
                                           final_destination->id,
                                           final_destination->id);
    (void)PushEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, shipment->id,
                    origin->id, need_event != NULL ? need_event->id :
                    LatestLocalCause(sim, final_destination->id),
                    quantity, text);
}

static void PlanTrade(CcSim *sim)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t route_used[CC_MAX_ROUTES] = {0};
        for (int32_t transfer = 0; transfer < sim->settlement_count * 2; ++transfer) {
            int32_t best_score = 0;
            int32_t best_source = -1;
            int32_t best_destination = -1;
            int32_t best_route = -1;
            CcId best_hop = 0U;
            for (int32_t destination = 0; destination < sim->settlement_count; ++destination) {
                CcSettlement *to = &sim->settlements[destination];
                int32_t need = to->reserve_target[good] - to->stock[good] -
                               CcSimIncomingGood(sim, to->id, (CcGood)good);
                if (need < 4) continue;
                for (int32_t source = 0; source < sim->settlement_count; ++source) {
                    if (source == destination) continue;
                    CcSettlement *from = &sim->settlements[source];
                    int32_t surplus = from->stock[good] - from->reserve_target[good];
                    if (surplus < 4) continue;
                    int32_t route_slot = -1;
                    CcId next_hop = 0U;
                    int32_t path_cost = 0;
                    if (!FindTradePath(sim, from->id, to->id, &route_slot,
                                       &next_hop, &path_cost, route_used)) continue;
                    int32_t score = need * 3 + surplus +
                                    ((CcGood)good == CC_GOOD_FOOD ? to->hunger * 2 : 0) -
                                    path_cost / 3;
                    if (score > best_score) {
                        best_score = score;
                        best_source = source;
                        best_destination = destination;
                        best_route = route_slot;
                        best_hop = next_hop;
                    }
                }
            }
            if (best_source < 0 || best_destination < 0 || best_route < 0) break;
            CreateTradeShipment(sim, best_route, best_hop, (CcGood)good,
                                &sim->settlements[best_source],
                                &sim->settlements[best_destination], route_used);
        }
    }
}

static CcFaction *FactionFor(CcSim *sim, CcId kingdom_id, CcFactionKind kind)
{
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].kingdom_id == kingdom_id && sim->factions[i].kind == kind) {
            return &sim->factions[i];
        }
    }
    return NULL;
}

static CcSituation *AllocateSituation(CcSim *sim)
{
    if (sim->situation_count < CC_MAX_SITUATIONS) {
        CcSituation *situation = &sim->situations[sim->situation_count];
        sim->situation_count += 1;
        *situation = (CcSituation){0};
        return situation;
    }
    int32_t oldest = -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) continue;
        if (oldest < 0 || sim->situations[i].created_day < sim->situations[oldest].created_day) {
            oldest = i;
        }
    }
    if (oldest < 0) return NULL;
    sim->situations[oldest] = (CcSituation){0};
    return &sim->situations[oldest];
}

static bool HasRecentSituation(const CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->kind == kind && situation->target_id == target &&
            (situation->status == CC_SITUATION_ACTIVE ||
             situation->created_day >= sim->current_day - 28)) return true;
    }
    return false;
}

static void CreateSituation(CcSim *sim, CcSituationKind kind, CcId target,
                            CcId issuer, CcId cause, CcGood good,
                            int32_t quantity, CcMoney reward, int32_t duration)
{
    if (HasRecentSituation(sim, kind, target)) return;
    CcSituation *situation = AllocateSituation(sim);
    if (situation == NULL) return;
    situation->id = NextId(sim, CC_ENTITY_SITUATION);
    situation->kind = kind;
    situation->status = CC_SITUATION_ACTIVE;
    situation->issuer_faction_id = issuer;
    situation->target_id = target;
    situation->cause_event_id = cause;
    situation->good = good;
    situation->quantity = quantity;
    situation->reward = reward;
    situation->created_day = sim->current_day;
    situation->deadline_day = sim->current_day + duration;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s issued; reward %" PRId64
                   " crowns before day %d.", CcSituationKindName(kind), reward,
                   situation->deadline_day);
    (void)PushEvent(sim, CC_EVENT_SITUATION_CREATED, situation->id, target,
                    cause, quantity, text);
}

static void GenerateSituations(CcSim *sim)
{
    CcSettlement *relief_target = NULL;
    int32_t relief_need = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        int32_t projected = settlement->stock[CC_GOOD_FOOD] +
                            CcSimIncomingGood(sim, settlement->id, CC_GOOD_FOOD);
        int32_t need = settlement->reserve_target[CC_GOOD_FOOD] - projected +
                       settlement->hunger;
        if (need > relief_need && (projected < settlement->reserve_target[CC_GOOD_FOOD] / 2 ||
                                   settlement->hunger >= 25)) {
            relief_need = need;
            relief_target = settlement;
        }
    }
    if (relief_target != NULL) {
        CcFaction *issuer = FactionFor(sim, relief_target->kingdom_id, CC_FACTION_COMMONS);
        const CcEvent *shortage = LatestEvent(sim, CC_EVENT_SHORTAGE,
                                              relief_target->id, relief_target->id);
        CreateSituation(sim, CC_SITUATION_RELIEF_DELIVERY, relief_target->id,
                        issuer != NULL ? issuer->id : 0U,
                        shortage != NULL ? shortage->id : LatestLocalCause(sim, relief_target->id),
                        CC_GOOD_FOOD, 8, 30, 35);
    }

    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (!route->closed) continue;
        CcSettlement *place = CcSimSettlementMutable(sim, route->to_id);
        CcFaction *issuer = place != NULL ?
            FactionFor(sim, place->kingdom_id, CC_FACTION_CROWN) : NULL;
        const CcEvent *closure = LatestEvent(sim, CC_EVENT_ROUTE_CLOSED, route->id, 0U);
        CreateSituation(sim, CC_SITUATION_ROUTE_REPAIR, route->id,
                        issuer != NULL ? issuer->id : 0U,
                        closure != NULL ? closure->id : 0U,
                        CC_GOOD_TOOLS, 3, 36, 49);
    }

    for (int32_t i = 0; i < sim->monster_count; ++i) {
        CcMonsterPopulation *monster = &sim->monsters[i];
        if (monster->pressure < 35) continue;
        CcDungeon *dungeon = DungeonByIdMutable(sim, monster->dungeon_id);
        CcSettlement *place = dungeon != NULL ?
            CcSimSettlementMutable(sim, dungeon->settlement_id) : NULL;
        CcFaction *issuer = place != NULL ?
            FactionFor(sim, place->kingdom_id, CC_FACTION_GUILD) : NULL;
        const CcEvent *pressure = LatestEvent(sim, CC_EVENT_MONSTER_PRESSURE,
                                             monster->id, monster->dungeon_id);
        if (dungeon != NULL) {
            CreateSituation(sim, CC_SITUATION_MONSTER_EXPEDITION, dungeon->id,
                            issuer != NULL ? issuer->id : 0U,
                            pressure != NULL ? pressure->id : 0U,
                            CC_GOOD_TOOLS, 0, 42, 42);
        }
    }

    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        const CcRoute *route = CcSimRoute(sim, bandits->route_id);
        if (route == NULL || !route->smuggler_route || bandits->influence < 42) continue;
        CcSettlement *target = CcSimSettlementMutable(sim, route->to_id);
        CcFaction *issuer = target != NULL ?
            FactionFor(sim, target->kingdom_id, CC_FACTION_GUILD) : NULL;
        const CcEvent *pressure = LatestEvent(sim, CC_EVENT_BANDIT_PRESSURE,
                                             bandits->id, route->id);
        if (target != NULL) {
            CreateSituation(sim, CC_SITUATION_BLACK_MARKET_DELIVERY, target->id,
                            issuer != NULL ? issuer->id : 0U,
                            pressure != NULL ? pressure->id : 0U,
                            CC_GOOD_TOOLS, 5, 34, 28);
        }
    }
}

static void ResolveSituation(CcSim *sim, CcSituation *situation)
{
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) return;
    situation->status = CC_SITUATION_RESOLVED;
    sim->player.coins += situation->reward;
    sim->player.reputation += 4 + situation->quantity / 2;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s fulfilled; the company receives %" PRId64
                   " crowns.", CcSituationKindName(situation->kind), situation->reward);
    (void)PushEvent(sim, CC_EVENT_SITUATION_RESOLVED, situation->id,
                    situation->target_id, situation->cause_event_id,
                    (int32_t)situation->reward, text);
}

static void ProgressDeliverySituations(CcSim *sim, CcId settlement_id,
                                       CcGood good, int32_t quantity)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->target_id != settlement_id || situation->good != good) continue;
        if (situation->kind != CC_SITUATION_RELIEF_DELIVERY &&
            situation->kind != CC_SITUATION_BLACK_MARKET_DELIVERY) continue;
        situation->progress = MinimumI32(situation->quantity,
                                         situation->progress + quantity);
        if (situation->progress >= situation->quantity) ResolveSituation(sim, situation);
    }
}

static void ResolveTargetSituations(CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status == CC_SITUATION_ACTIVE && situation->kind == kind &&
            situation->target_id == target) ResolveSituation(sim, situation);
    }
}

static void SupersedeTargetSituations(CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE || situation->kind != kind ||
            situation->target_id != target) continue;
        situation->status = CC_SITUATION_FAILED;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s withdrawn after another power intervenes.",
                       CcSituationKindName(situation->kind));
        (void)PushEvent(sim, CC_EVENT_SITUATION_FAILED, situation->id,
                        situation->target_id, situation->cause_event_id, 0, text);
    }
}

static void ExpireSituations(CcSim *sim)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->deadline_day >= sim->current_day) continue;
        situation->status = CC_SITUATION_FAILED;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s expires unanswered; its sponsors remember.",
                       CcSituationKindName(situation->kind));
        (void)PushEvent(sim, CC_EVENT_SITUATION_FAILED, situation->id,
                        situation->target_id, situation->cause_event_id, 0, text);
        sim->player.reputation -= 1;
    }
}

static void UpdateThreats(CcSim *sim)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        const CcRoute *route = CcSimRoute(sim, bandits->route_id);
        const CcSettlement *a = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
        const CcSettlement *b = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
        int32_t hunger = (a != NULL ? a->hunger : 0) + (b != NULL ? b->hunger : 0);
        bandits->members = ClampI32(bandits->members + hunger / 45 - 1, 4, 120);
        bandits->supplies = ClampI32(bandits->supplies - 2, 0, 100);
        bandits->influence = ClampI32((bandits->members + bandits->supplies) / 2,
                                      0, 100);
        CcRoute *mutable_route = RouteMutable(sim, bandits->route_id);
        if (mutable_route != NULL) {
            mutable_route->security = ClampI32(mutable_route->security -
                (bandits->influence >= 65 ? 2 : bandits->influence >= 45 ? 1 : 0),
                5, 100);
        }
        int32_t level = bandits->influence >= 70 ? 3 :
                        bandits->influence >= 50 ? 2 :
                        bandits->influence >= 30 ? 1 : 0;
        if (level > sim->last_bandit_level[i]) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%s recruits from hungry settlements; road control reaches %d%%.",
                           bandits->name, bandits->influence);
            const CcEvent *loss = LatestEvent(sim, CC_EVENT_SHIPMENT_LOST, 0U,
                                              b != NULL ? b->id : 0U);
            (void)PushEvent(sim, CC_EVENT_BANDIT_PRESSURE, bandits->id,
                            bandits->route_id, loss != NULL ? loss->id : 0U,
                            bandits->influence, text);
        }
        sim->last_bandit_level[i] = level;
    }

    for (int32_t i = 0; i < sim->monster_count; ++i) {
        CcMonsterPopulation *monster = &sim->monsters[i];
        CcDungeon *dungeon = NULL;
        for (int32_t d = 0; d < sim->dungeon_count; ++d) {
            if (sim->dungeons[d].id == monster->dungeon_id) dungeon = &sim->dungeons[d];
        }
        int32_t growth = dungeon != NULL && dungeon->state == CC_DUNGEON_DISTURBED ? 3 :
                         dungeon != NULL && dungeon->state == CC_DUNGEON_RESEALED ? -3 : 1;
        monster->population = ClampI32(monster->population + growth -
                                       monster->hunting_pressure / 20, 0, 160);
        monster->hunting_pressure = ClampI32(monster->hunting_pressure - 4, 0, 100);
        if (dungeon != NULL) {
            int32_t pressure_delta = dungeon->state == CC_DUNGEON_DISTURBED ? 2 :
                                     dungeon->state == CC_DUNGEON_SMUGGLER_ROUTE ? 1 :
                                     dungeon->state == CC_DUNGEON_RESEALED ? -4 : -1;
            dungeon->regional_pressure = ClampI32(dungeon->regional_pressure +
                                                  pressure_delta, 0, 100);
        }
        monster->pressure = ClampI32(monster->population / 2 +
                                     (dungeon != NULL ? dungeon->regional_pressure / 2 : 0),
                                     0, 100);
        int32_t level = monster->pressure >= 70 ? 3 :
                        monster->pressure >= 50 ? 2 :
                        monster->pressure >= 30 ? 1 : 0;
        if (level > sim->last_monster_level[i]) {
            char text[CC_EVENT_TEXT_CAPACITY];
            const CcSettlement *place = dungeon != NULL ?
                CcSimSettlement(sim, dungeon->settlement_id) : NULL;
            (void)snprintf(text, sizeof(text), "%s breach another shaft beneath %s.",
                           monster->name, place != NULL ? place->name : "the frontier");
            const CcEvent *change = dungeon != NULL ?
                LatestEvent(sim, CC_EVENT_DUNGEON_CHANGED, dungeon->id,
                            dungeon->settlement_id) : NULL;
            (void)PushEvent(sim, CC_EVENT_MONSTER_PRESSURE, monster->id,
                            monster->dungeon_id, change != NULL ? change->id : 0U,
                            monster->pressure, text);
        }
        sim->last_monster_level[i] = level;
    }
}

static int32_t ActiveShipmentsForKingdom(const CcSim *sim, CcId kingdom_id)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        const CcSettlement *origin = CcSimSettlement(sim, shipment->origin_id);
        if (shipment->status == CC_SHIPMENT_TRAVELLING && origin != NULL &&
            origin->kingdom_id == kingdom_id) count += 1;
    }
    return count;
}

static void UpdateRoutesAndGovernments(CcSim *sim)
{
    for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
        CcRoute *route = &sim->routes[route_index];
        if (!route->closed && sim->current_day % 28 == 0) {
            int32_t decay = route->smuggler_route ? 2 : 1;
            route->condition = ClampI32(route->condition - decay, 0, 100);
            if (route->condition < 22) {
                route->closed = true;
                char text[CC_EVENT_TEXT_CAPACITY];
                const CcSettlement *from = CcSimSettlement(sim, route->from_id);
                const CcSettlement *to = CcSimSettlement(sim, route->to_id);
                (void)snprintf(text, sizeof(text),
                               "The %s-%s road fails after years of deferred maintenance.",
                               from != NULL ? from->name : "western",
                               to != NULL ? to->name : "eastern");
                (void)PushEvent(sim, CC_EVENT_ROUTE_DECAY, route->id, route->to_id,
                                LatestLocalCause(sim, route->to_id), route->condition, text);
            }
        }
    }

    for (int32_t kingdom_index = 0; kingdom_index < sim->kingdom_count; ++kingdom_index) {
        CcKingdom *kingdom = &sim->kingdoms[kingdom_index];
        int32_t settlements = 0;
        int32_t hunger_sum = 0;
        int32_t prosperity_sum = 0;
        CcSettlement *worst = NULL;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *settlement = &sim->settlements[i];
            if (settlement->kingdom_id != kingdom->id) continue;
            settlements += 1;
            hunger_sum += settlement->hunger;
            prosperity_sum += settlement->prosperity;
            if (worst == NULL || settlement->hunger > worst->hunger) worst = settlement;
        }
        int32_t average_hunger = settlements > 0 ? hunger_sum / settlements : 0;
        int32_t average_prosperity = settlements > 0 ? prosperity_sum / settlements : 0;
        int32_t revenue = MaximumI32(1, average_prosperity / 18) +
                          ActiveShipmentsForKingdom(sim, kingdom->id);
        kingdom->treasury += revenue;
        kingdom->legitimacy = ClampI32(kingdom->legitimacy +
            (average_hunger > 55 ? -2 : average_hunger > 30 ? -1 :
             average_hunger < 12 && average_prosperity > 55 ? 1 : 0), 0, 100);

        CcFaction *crown = FactionFor(sim, kingdom->id, CC_FACTION_CROWN);
        CcFaction *guild = FactionFor(sim, kingdom->id, CC_FACTION_GUILD);
        CcFaction *commons = FactionFor(sim, kingdom->id, CC_FACTION_COMMONS);
        int32_t shipments = ActiveShipmentsForKingdom(sim, kingdom->id);
        if (crown != NULL) crown->support = ClampI32(crown->support +
            (average_hunger < 20 ? 1 : -2), 0, 100);
        if (guild != NULL) guild->support = ClampI32(guild->support +
            (shipments > 0 ? 1 : -1), 0, 100);
        if (commons != NULL) commons->support = ClampI32(commons->support +
            (average_hunger > 25 ? 2 : -1), 0, 100);
        CcFaction *factions[3] = {crown, guild, commons};
        for (int32_t i = 0; i < 3; ++i) {
            if (factions[i] == NULL) continue;
            int32_t difference = factions[i]->support - factions[i]->power;
            factions[i]->power = ClampI32(factions[i]->power +
                (AbsoluteI32(difference) > 3 ? (difference > 0 ? 1 : -1) : 0), 0, 100);
        }
        if (commons != NULL && crown != NULL &&
            commons->support > 72 && crown->support < 38) {
            kingdom->legitimacy = ClampI32(kingdom->legitimacy - 2, 0, 100);
        }

        if (sim->current_day % 28 == 0 && worst != NULL && worst->hunger >= 38 &&
            kingdom->treasury >= 28) {
            kingdom->treasury -= 28;
            int32_t grain = 18 + worst->hunger / 4;
            worst->stock[CC_GOOD_FOOD] += grain;
            worst->hunger = ClampI32(worst->hunger - 8, 0, 100);
            kingdom->legitimacy = ClampI32(kingdom->legitimacy + 2, 0, 100);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%s spends 28 crowns on %d emergency grain for %s.",
                           kingdom->name, grain, worst->name);
            const CcEvent *shortage = LatestEvent(sim, CC_EVENT_SHORTAGE,
                                                  worst->id, worst->id);
            (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, worst->id,
                            shortage != NULL ? shortage->id : 0U, grain, text);
        }

        if (sim->current_day % 28 == 0 && kingdom->treasury >= 34) {
            for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
                CcRoute *route = &sim->routes[route_index];
                CcSettlement *to = CcSimSettlementMutable(sim, route->to_id);
                if (!route->closed || to == NULL || to->kingdom_id != kingdom->id) continue;
                kingdom->treasury -= 34;
                route->condition = ClampI32(route->condition + 21, 0, 100);
                if (route->condition >= 70) {
                    route->closed = false;
                    route->security = ClampI32(route->security + 6, 0, 100);
                    SupersedeTargetSituations(sim, CC_SITUATION_ROUTE_REPAIR, route->id);
                }
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s funds works on the closed road; condition rises to %d.",
                               kingdom->name, route->condition);
                (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, route->id,
                                LatestLocalCause(sim, route->to_id), route->condition, text);
                break;
            }
        }

        if (sim->current_day % 28 == 0 && kingdom->treasury >= 12) {
            for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
                CcRoute *route = &sim->routes[route_index];
                CcSettlement *to = CcSimSettlementMutable(sim, route->to_id);
                if (route->closed || route->condition >= 58 || to == NULL ||
                    to->kingdom_id != kingdom->id) continue;
                kingdom->treasury -= 12;
                route->condition = ClampI32(route->condition + 16, 0, 100);
                route->security = ClampI32(route->security + 2, 0, 100);
                if (route->condition < 45) {
                    char text[CC_EVENT_TEXT_CAPACITY];
                    (void)snprintf(text, sizeof(text),
                                   "%s maintains a strategic road before it fails.",
                                   kingdom->name);
                    (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, route->id,
                                    0U, route->condition, text);
                }
                break;
            }
        }

        if (sim->current_day % 28 == 0) {
            CcMonsterPopulation *worst_monster = NULL;
            CcDungeon *worst_dungeon = NULL;
            for (int32_t monster_index = 0; monster_index < sim->monster_count; ++monster_index) {
                CcMonsterPopulation *monster = &sim->monsters[monster_index];
                CcDungeon *dungeon = DungeonByIdMutable(sim, monster->dungeon_id);
                CcSettlement *place = dungeon != NULL ?
                    CcSimSettlementMutable(sim, dungeon->settlement_id) : NULL;
                if (place == NULL || place->kingdom_id != kingdom->id || monster->pressure < 72) continue;
                if (worst_monster == NULL || monster->pressure > worst_monster->pressure) {
                    worst_monster = monster;
                    worst_dungeon = dungeon;
                }
            }
            if (worst_monster != NULL && worst_dungeon != NULL) {
                CcMoney hunt_cost = kingdom->treasury >= 18 ? 18 : kingdom->treasury;
                kingdom->treasury -= hunt_cost;
                worst_monster->hunting_pressure = ClampI32(
                    worst_monster->hunting_pressure + 36, 0, 100);
                worst_monster->population = ClampI32(worst_monster->population - 12, 0, 160);
                worst_monster->pressure = ClampI32(worst_monster->pressure - 18, 0, 100);
                worst_dungeon->regional_pressure = ClampI32(
                    worst_dungeon->regional_pressure - 9, 0, 100);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s musters a deep hunt; %s pressure falls to %d.",
                               guild != NULL ? guild->name : kingdom->name,
                               worst_monster->name, worst_monster->pressure);
                const CcEvent *pressure = LatestEvent(sim, CC_EVENT_MONSTER_PRESSURE,
                                                     worst_monster->id,
                                                     worst_monster->dungeon_id);
                (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id,
                                worst_dungeon->id,
                                pressure != NULL ? pressure->id : 0U,
                                worst_monster->pressure, text);
            }
        }

        if (sim->current_day % 28 == 0) {
            CcFaction *leader = crown;
            if (guild != NULL && (leader == NULL || guild->support > leader->support)) leader = guild;
            if (commons != NULL && (leader == NULL || commons->support > leader->support)) leader = commons;
            if (leader != NULL) {
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s leads public support in %s at %d%%.",
                               leader->name, kingdom->name, leader->support);
                (void)PushEvent(sim, CC_EVENT_FACTION_SHIFT, leader->id, kingdom->id,
                                0U, leader->support, text);
            }
        }
    }
}

void CcSimAdvanceDays(CcSim *sim, int32_t days)
{
    if (sim == NULL || days <= 0) return;
    for (int32_t day = 0; day < days; ++day) {
        sim->current_day += 1;
        UpdateShipments(sim);
        if (sim->current_day % 7 == 0) {
            for (int32_t settlement = 0; settlement < sim->settlement_count; ++settlement) {
                UpdateSettlement(sim, settlement);
            }
            PlanTrade(sim);
            UpdateThreats(sim);
            UpdateRoutesAndGovernments(sim);
            ExpireSituations(sim);
            GenerateSituations(sim);
        }
    }
}

static bool ApplyTrade(CcSim *sim, const CcCommand *command,
                       char *error, size_t error_capacity)
{
    if (command->good < 0 || command->good >= CC_GOOD_COUNT || command->amount == 0) {
        SetError(error, error_capacity, "Trade command is invalid.");
        return false;
    }
    CcSettlement *settlement = CcSimSettlementMutable(sim, sim->player.location_id);
    if (settlement == NULL) {
        SetError(error, error_capacity, "The carriage is not at a settlement.");
        return false;
    }
    int32_t amount = command->amount;
    int32_t price = settlement->price[command->good];
    if (amount > 0) {
        if (settlement->stock[command->good] < amount) {
            SetError(error, error_capacity, "The local market lacks that stock.");
            return false;
        }
        if (CcPlayerCargoUsed(&sim->player) + amount > sim->player.cargo_capacity) {
            SetError(error, error_capacity, "The carriage has no cargo space.");
            return false;
        }
        CcMoney cost = (CcMoney)amount * (CcMoney)price;
        if (sim->player.coins < cost) {
            SetError(error, error_capacity, "The company cannot afford that cargo.");
            return false;
        }
        sim->player.coins -= cost;
        settlement->stock[command->good] -= amount;
        sim->player.cargo[command->good] += amount;
    } else {
        int32_t selling = -amount;
        if (sim->player.cargo[command->good] < selling) {
            SetError(error, error_capacity, "The carriage does not carry that cargo.");
            return false;
        }
        sim->player.cargo[command->good] -= selling;
        settlement->stock[command->good] += selling;
        sim->player.coins += (CcMoney)selling * (CcMoney)price;
        if (command->good == CC_GOOD_FOOD && settlement->hunger > 0) {
            settlement->hunger = ClampI32(settlement->hunger - selling * 2, 0, 100);
            sim->player.reputation += selling;
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "The Crownless company %s %d %s at %s.",
                   amount > 0 ? "loads" : "delivers", amount > 0 ? amount : -amount,
                   CcGoodName(command->good), settlement->name);
    (void)PushEvent(sim, command->good == CC_GOOD_FOOD && amount < 0 ?
                    CC_EVENT_RELIEF : CC_EVENT_PLAYER_TRADE,
                    sim->player.id, settlement->id, 0,
                    amount > 0 ? amount : -amount, text);
    if (amount < 0) {
        ProgressDeliverySituations(sim, settlement->id, command->good, -amount);
    }
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyTravel(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity)
{
    const CcSettlement *destination = CcSimSettlement(sim, command->target_id);
    if (destination == NULL) {
        SetError(error, error_capacity, "That destination does not exist.");
        return false;
    }
    const CcRoute *route = CcSimRouteBetween(sim, sim->player.location_id,
                                             destination->id);
    if (route == NULL) {
        SetError(error, error_capacity, "No direct carriage route connects those places.");
        return false;
    }
    if (route->closed) {
        SetError(error, error_capacity, "The route is closed by current events.");
        return false;
    }
    CcId origin = sim->player.location_id;
    int32_t days = route->travel_days;
    int32_t fare = days + (route->smuggler_route ? 3 : 0);
    if (sim->player.coins < fare) {
        SetError(error, error_capacity, "The company cannot provision that journey.");
        return false;
    }
    int32_t danger = CcSimRouteDanger(sim, route->id);
    bool ambushed = (int32_t)(NextRandom(sim) % 100U) < danger / 2;
    CcSimAdvanceDays(sim, days);
    sim->player.location_id = destination->id;
    sim->player.coins -= fare;
    CcEvent *ambush_event = NULL;
    if (ambushed) {
        int32_t stolen = 0;
        CcGood stolen_good = CC_GOOD_COUNT;
        for (int32_t good = CC_GOOD_COUNT - 1; good >= 0; --good) {
            if (sim->player.cargo[good] <= stolen) continue;
            stolen = sim->player.cargo[good];
            stolen_good = (CcGood)good;
        }
        char ambush_text[CC_EVENT_TEXT_CAPACITY];
        if (stolen_good != CC_GOOD_COUNT && stolen > 0) {
            int32_t quantity = MinimumI32(2, stolen);
            sim->player.cargo[stolen_good] -= quantity;
            CcBanditGroup *bandits = BanditsOnRoute(sim, route->id);
            if (bandits != NULL) bandits->supplies = ClampI32(bandits->supplies + quantity, 0, 100);
            (void)snprintf(ambush_text, sizeof(ambush_text),
                           "Roadside attackers take %d %s; this journey wanted a local encounter.",
                           quantity, CcGoodName(stolen_good));
            stolen = quantity;
        } else {
            stolen = MinimumI32(6, (int32_t)sim->player.coins);
            sim->player.coins -= stolen;
            (void)snprintf(ambush_text, sizeof(ambush_text),
                           "Roadside attackers take %d crowns; this journey wanted a local encounter.",
                           stolen);
        }
        ambush_event = PushEvent(sim, CC_EVENT_PLAYER_AMBUSH, sim->player.id,
                                 route->id, LatestLocalCause(sim, destination->id),
                                 stolen, ambush_text);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    const CcSettlement *from = CcSimSettlement(sim, origin);
    (void)snprintf(text, sizeof(text),
                   "The Crownless carriage reaches %s from %s after %d days (%d%% danger).",
                   destination->name, from != NULL ? from->name : "the road", days, danger);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id,
                    destination->id, ambush_event != NULL ? ambush_event->id : 0U,
                    days, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyRepair(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity)
{
    CcRoute *route = RouteMutable(sim, command->target_id);
    if (route == NULL || !route->closed) {
        SetError(error, error_capacity, "That route does not require repair.");
        return false;
    }
    if (sim->player.location_id != route->from_id &&
        sim->player.location_id != route->to_id) {
        SetError(error, error_capacity, "The carriage must reach an end of the route first.");
        return false;
    }
    if (sim->player.cargo[CC_GOOD_TOOLS] >= 3) {
        sim->player.cargo[CC_GOOD_TOOLS] -= 3;
    } else if (sim->player.coins >= 18) {
        sim->player.coins -= 18;
    } else {
        SetError(error, error_capacity, "Repair requires 3 tools or 18 crowns.");
        return false;
    }
    CcSimAdvanceDays(sim, 2);
    route = RouteMutable(sim, command->target_id);
    if (route == NULL) return false;
    route->closed = false;
    route->condition = 84;
    route->security = ClampI32(route->security + 8, 0, 100);
    (void)PushEvent(sim, CC_EVENT_ROUTE_REPAIRED, route->id,
                    sim->player.location_id, 0, 84,
                    "The Crownless company reopens the treaty bridge to traffic.");
    sim->player.reputation += 8;
    ResolveTargetSituations(sim, CC_SITUATION_ROUTE_REPAIR, route->id);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyDungeonChange(CcSim *sim, const CcCommand *command,
                               char *error, size_t error_capacity)
{
    CcDungeon *dungeon = NULL;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == command->target_id) dungeon = &sim->dungeons[i];
    }
    if (dungeon == NULL || sim->player.location_id != dungeon->settlement_id) {
        SetError(error, error_capacity, "The company must prepare at the dungeon settlement.");
        return false;
    }
    if (command->dungeon_state < CC_DUNGEON_SEALED ||
        command->dungeon_state > CC_DUNGEON_RESEALED) {
        SetError(error, error_capacity, "That dungeon outcome is invalid.");
        return false;
    }
    CcSimAdvanceDays(sim, 3);
    dungeon->state = command->dungeon_state;
    dungeon->regional_pressure = command->dungeon_state == CC_DUNGEON_RESEALED ? 12 :
                                 command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ? 28 :
                                 command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ? 52 : 36;
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        if (sim->monsters[i].dungeon_id != dungeon->id) continue;
        sim->monsters[i].hunting_pressure = ClampI32(sim->monsters[i].hunting_pressure + 35,
                                                     0, 100);
        sim->monsters[i].population = ClampI32(sim->monsters[i].population - 12, 0, 160);
        sim->monsters[i].pressure = ClampI32(sim->monsters[i].pressure - 18, 0, 100);
    }
    (void)PushEvent(sim, CC_EVENT_DUNGEON_CHANGED, dungeon->id,
                    dungeon->settlement_id, 0, dungeon->regional_pressure,
                    "An expedition changes the dungeon, its ecology, and the regional economy.");
    ResolveTargetSituations(sim, CC_SITUATION_MONSTER_EXPEDITION, dungeon->id);
    SetError(error, error_capacity, "");
    return true;
}

bool CcSimApply(CcSim *sim, const CcCommand *command,
                char *error, size_t error_capacity)
{
    if (sim == NULL || command == NULL) {
        SetError(error, error_capacity, "Command target is missing.");
        return false;
    }
    switch (command->kind) {
        case CC_COMMAND_TRADE:
            return ApplyTrade(sim, command, error, error_capacity);
        case CC_COMMAND_TRAVEL:
            return ApplyTravel(sim, command, error, error_capacity);
        case CC_COMMAND_REPAIR_ROUTE:
            return ApplyRepair(sim, command, error, error_capacity);
        case CC_COMMAND_CHANGE_DUNGEON:
            return ApplyDungeonChange(sim, command, error, error_capacity);
        case CC_COMMAND_NONE:
            break;
    }
    SetError(error, error_capacity, "Command kind is invalid.");
    return false;
}

bool CcSimValidate(const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim == NULL) {
        SetError(error, error_capacity, "Simulation is missing.");
        return false;
    }
    if (sim->schema_version != CC_SIM_SCHEMA_VERSION ||
        sim->generator_version != CC_GENERATOR_VERSION) {
        SetError(error, error_capacity, "Simulation version is unsupported.");
        return false;
    }
    if (sim->kingdom_count < 1 || sim->kingdom_count > CC_MAX_KINGDOMS ||
        sim->settlement_count < 1 || sim->settlement_count > CC_MAX_SETTLEMENTS ||
        sim->route_count < 0 || sim->route_count > CC_MAX_ROUTES ||
        sim->faction_count < 0 || sim->faction_count > CC_MAX_FACTIONS ||
        sim->shipment_count < 0 || sim->shipment_count > CC_MAX_SHIPMENTS ||
        sim->bandit_count < 0 || sim->bandit_count > CC_MAX_BANDITS ||
        sim->monster_count < 0 || sim->monster_count > CC_MAX_MONSTERS ||
        sim->dungeon_count < 0 || sim->dungeon_count > CC_MAX_DUNGEONS ||
        sim->situation_count < 0 || sim->situation_count > CC_MAX_SITUATIONS ||
        sim->event_count < 0 || sim->event_count > CC_MAX_EVENTS) {
        SetError(error, error_capacity, "Simulation counts are invalid.");
        return false;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *settlement = &sim->settlements[i];
        if (CcIdKind(settlement->id) != CC_ENTITY_SETTLEMENT) {
            SetError(error, error_capacity, "Settlement ID is invalid.");
            return false;
        }
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (settlement->stock[good] < 0 || settlement->price[good] < 1) {
                SetError(error, error_capacity, "Market accounting is invalid.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (CcIdKind(route->id) != CC_ENTITY_ROUTE ||
            CcSimSettlement(sim, route->from_id) == NULL ||
            CcSimSettlement(sim, route->to_id) == NULL ||
            route->travel_days < 1 || route->capacity < 1) {
            SetError(error, error_capacity, "Route data is invalid.");
            return false;
        }
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        if (CcIdKind(shipment->id) != CC_ENTITY_SHIPMENT ||
            CcSimSettlement(sim, shipment->origin_id) == NULL ||
            CcSimSettlement(sim, shipment->destination_id) == NULL ||
            CcSimSettlement(sim, shipment->final_destination_id) == NULL ||
            CcSimRoute(sim, shipment->route_id) == NULL ||
            shipment->good < 0 || shipment->good >= CC_GOOD_COUNT ||
            shipment->quantity < 1 || shipment->status < CC_SHIPMENT_UNUSED ||
            shipment->status > CC_SHIPMENT_LOST) {
            SetError(error, error_capacity, "Shipment data is invalid.");
            return false;
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        CcEntityKind target_kind = CcIdKind(situation->target_id);
        CcEntityKind expected_kind = situation->kind == CC_SITUATION_ROUTE_REPAIR ?
                                     CC_ENTITY_ROUTE :
                                     situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                                     CC_ENTITY_DUNGEON : CC_ENTITY_SETTLEMENT;
        if (CcIdKind(situation->id) != CC_ENTITY_SITUATION ||
            target_kind != expected_kind ||
            (situation->issuer_faction_id != 0U &&
             CcIdKind(situation->issuer_faction_id) != CC_ENTITY_FACTION) ||
            situation->status < CC_SITUATION_ACTIVE ||
            situation->status > CC_SITUATION_FAILED ||
            situation->quantity < 0 || situation->progress < 0 ||
            situation->progress > situation->quantity || situation->reward < 0 ||
            situation->deadline_day < situation->created_day) {
            SetError(error, error_capacity, "Situation data is invalid.");
            return false;
        }
    }
    if (CcSimSettlement(sim, sim->player.location_id) == NULL ||
        CcPlayerCargoUsed(&sim->player) > sim->player.cargo_capacity ||
        sim->player.coins < 0) {
        SetError(error, error_capacity, "Player company state is invalid.");
        return false;
    }
    SetError(error, error_capacity, "");
    return true;
}

static uint64_t HashU64(uint64_t hash, uint64_t value)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= value & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static uint64_t HashString(uint64_t hash, const char *text)
{
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= UINT64_C(1099511628211);
        text += 1;
    }
    return HashU64(hash, 0U);
}

uint64_t CcSimHash(const CcSim *sim)
{
    if (sim == NULL) return 0U;
    uint64_t hash = UINT64_C(1469598103934665603);
#define HASH_VALUE(value) hash = HashU64(hash, (uint64_t)(value))
    HASH_VALUE(sim->schema_version);
    HASH_VALUE(sim->generator_version);
    HASH_VALUE(sim->world_seed);
    HASH_VALUE(sim->random_state);
    HASH_VALUE(sim->current_day);
    HASH_VALUE(sim->next_entity_serial);
    HASH_VALUE(sim->kingdom_count);
    HASH_VALUE(sim->settlement_count);
    HASH_VALUE(sim->route_count);
    HASH_VALUE(sim->faction_count);
    HASH_VALUE(sim->shipment_count);
    HASH_VALUE(sim->bandit_count);
    HASH_VALUE(sim->monster_count);
    HASH_VALUE(sim->dungeon_count);
    HASH_VALUE(sim->situation_count);
    HASH_VALUE(sim->event_count);
    HASH_VALUE(sim->event_write_index);
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *item = &sim->kingdoms[i];
        HASH_VALUE(item->id); hash = HashString(hash, item->name);
        HASH_VALUE(item->color_r); HASH_VALUE(item->color_g); HASH_VALUE(item->color_b);
        HASH_VALUE(item->treasury); HASH_VALUE(item->legitimacy);
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *item = &sim->settlements[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
        hash = HashString(hash, item->name); HASH_VALUE(item->function);
        HASH_VALUE(item->map_x); HASH_VALUE(item->map_y); HASH_VALUE(item->population);
        HASH_VALUE(item->security); HASH_VALUE(item->prosperity); HASH_VALUE(item->hunger);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            HASH_VALUE(item->stock[good]); HASH_VALUE(item->reserve_target[good]);
            HASH_VALUE(item->production[good]); HASH_VALUE(item->consumption[good]);
            HASH_VALUE(item->price[good]);
        }
        HASH_VALUE(sim->last_shortage_level[i]);
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *item = &sim->routes[i];
        HASH_VALUE(item->id); HASH_VALUE(item->from_id); HASH_VALUE(item->to_id);
        HASH_VALUE(item->travel_days); HASH_VALUE(item->capacity); HASH_VALUE(item->security);
        HASH_VALUE(item->condition); HASH_VALUE(item->closed); HASH_VALUE(item->smuggler_route);
    }
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *item = &sim->factions[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->kind); HASH_VALUE(item->power); HASH_VALUE(item->support);
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *item = &sim->shipments[i];
        HASH_VALUE(item->id); HASH_VALUE(item->origin_id); HASH_VALUE(item->destination_id);
        HASH_VALUE(item->final_destination_id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->good); HASH_VALUE(item->quantity);
        HASH_VALUE(item->departure_day); HASH_VALUE(item->arrival_day); HASH_VALUE(item->status);
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *item = &sim->bandits[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->members); HASH_VALUE(item->supplies); HASH_VALUE(item->influence);
        HASH_VALUE(sim->last_bandit_level[i]);
    }
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *item = &sim->monsters[i];
        HASH_VALUE(item->id); HASH_VALUE(item->dungeon_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->population); HASH_VALUE(item->pressure);
        HASH_VALUE(item->hunting_pressure); HASH_VALUE(sim->last_monster_level[i]);
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *item = &sim->dungeons[i];
        HASH_VALUE(item->id); HASH_VALUE(item->settlement_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->state); HASH_VALUE(item->depth); HASH_VALUE(item->regional_pressure);
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *item = &sim->situations[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kind); HASH_VALUE(item->status);
        HASH_VALUE(item->issuer_faction_id); HASH_VALUE(item->target_id);
        HASH_VALUE(item->cause_event_id); HASH_VALUE(item->good);
        HASH_VALUE(item->quantity); HASH_VALUE(item->progress); HASH_VALUE(item->reward);
        HASH_VALUE(item->created_day); HASH_VALUE(item->deadline_day);
    }
    HASH_VALUE(sim->player.id); HASH_VALUE(sim->player.location_id);
    HASH_VALUE(sim->player.coins); HASH_VALUE(sim->player.cargo_capacity);
    HASH_VALUE(sim->player.passenger_capacity); HASH_VALUE(sim->player.reputation);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) HASH_VALUE(sim->player.cargo[good]);
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *item = &sim->events[i];
        HASH_VALUE(item->id); HASH_VALUE(item->day); HASH_VALUE(item->kind);
        HASH_VALUE(item->subject_id); HASH_VALUE(item->location_id); HASH_VALUE(item->parent_id);
        HASH_VALUE(item->magnitude); hash = HashString(hash, item->text);
    }
#undef HASH_VALUE
    return hash;
}
