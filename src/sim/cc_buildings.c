#include "sim/cc_buildings.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <string.h>

static uint32_t BuildingHash(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16);
}

static int32_t ClampPercent(int32_t value)
{
    return value < 0 ? 0 : value > 100 ? 100 : value;
}

uint32_t CcSettlementBlueprintForFunction(int32_t function)
{
    return function >= CC_SETTLEMENT_FARMING && function <= CC_SETTLEMENT_DUNGEON_TOWN ?
        (uint32_t)function + 1U : 0U;
}

int32_t CcSettlementBlueprintBuildingCount(uint32_t blueprint_id)
{
    static const int32_t counts[] = {0, 12, 11, 10, 9, 12, 9};
    return blueprint_id < sizeof(counts) / sizeof(counts[0]) ? counts[blueprint_id] : 0;
}

const CcBuildingState *CcSettlementBuilding(const CcSettlement *place, uint32_t plot_id)
{
    if (place == NULL || plot_id == 0U) return NULL;
    for (int32_t i = 0; i < place->building_count && i < CC_SETTLEMENT_BUILDING_CAPACITY; ++i) {
        if (place->buildings[i].plot_id == plot_id) return &place->buildings[i];
    }
    return NULL;
}

static int32_t TotalFire(const CcSettlement *place)
{
    int32_t total = 0;
    for (int32_t i = 0; i < place->building_count; ++i) total += place->buildings[i].fire_damage;
    return total;
}

/* A stable rank keeps the same fire front after blueprint or record reordering. */
static uint32_t FireKey(const CcBuildingState *house)
{
    return BuildingHash(house->style_seed ^ house->plot_id ^ UINT32_C(0xf17e));
}

static int32_t SelectFirePlot(const CcSettlement *place, bool repairing)
{
    int32_t choice = -1;
    for (int32_t i = 0; i < place->building_count; ++i) {
        const CcBuildingState *house = &place->buildings[i];
        if (repairing ? house->fire_damage <= 0 : house->fire_damage >= 100) continue;
        if (choice < 0) { choice = i; continue; }
        const CcBuildingState *best = &place->buildings[choice];
        bool active = house->repair_progress > 0 && house->repair_progress < 100;
        bool best_active = best->repair_progress > 0 && best->repair_progress < 100;
        if (repairing && active != best_active) {
            if (active) choice = i;
        } else if (repairing && house->fire_damage != best->fire_damage) {
            if (house->fire_damage < best->fire_damage) choice = i;
        } else if (FireKey(house) < FireKey(best) ||
                   (FireKey(house) == FireKey(best) && house->plot_id < best->plot_id)) {
            choice = i;
        }
    }
    return choice;
}

static void ReconcileFire(CcSettlement *place, int32_t day, bool funded)
{
    int32_t difference = ClampPercent(place->fire_damage) * place->building_count - TotalFire(place);
    while (difference != 0) {
        bool repairing = difference < 0;
        int32_t selected = SelectFirePlot(place, repairing);
        if (selected < 0) break;
        CcBuildingState *house = &place->buildings[selected];
        int32_t available = repairing ? house->fire_damage : 100 - house->fire_damage;
        int32_t amount = repairing ? -difference : difference;
        if (amount > available) amount = available;
        house->fire_damage += repairing ? -amount : amount;
        house->health = ClampPercent(house->health + (repairing ? amount : -amount));
        house->roof_health = ClampPercent(house->roof_health + (repairing ? amount : -amount));
        if (repairing && funded) {
            house->repair_progress = ClampPercent(house->repair_progress + amount);
            if (house->fire_damage > 0 && house->repair_progress == 100) house->repair_progress = 99;
            if (house->fire_damage == 0) {
                house->repair_progress = 100;
                house->health = 100;
                house->roof_health = 100;
                house->upkeep = 95;
            }
            house->last_repair_day = day;
        } else if (!repairing) {
            house->repair_progress = 0;
        }
        house->last_change_day = day;
        difference += repairing ? amount : -amount;
    }
}

float CcSettlementBuildingBurn(const CcSettlement *place, uint32_t plot_id)
{
    const CcBuildingState *house = CcSettlementBuilding(place, plot_id);
    if (house == NULL) return 0.0f;
    /* Capture fixtures and legacy callers may still set the town total. Resolve
       that pending change on a copy, so every view remains a pure read. */
    if (TotalFire(place) != ClampPercent(place->fire_damage) * place->building_count) {
        CcSettlement projected = *place;
        ReconcileFire(&projected, place->last_fire_day, false);
        house = CcSettlementBuilding(&projected, plot_id);
        return house != NULL ? (float)house->fire_damage / 100.0f : 0.0f;
    }
    return (float)house->fire_damage / 100.0f;
}

void CcBuildingsInitialize(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        CcSettlement *place = &sim->settlements[town];
        place->blueprint_id = CcSettlementBlueprintForFunction((int32_t)place->function);
        place->blueprint_version = CC_BUILDING_BLUEPRINT_VERSION;
        place->building_count = CcSettlementBlueprintBuildingCount(place->blueprint_id);
        memset(place->buildings, 0, sizeof(place->buildings));
        uint32_t seed = BuildingHash(sim->world_seed ^ (uint32_t)place->id ^ (uint32_t)(place->id >> 32));
        for (int32_t i = 0; i < place->building_count; ++i) {
            CcBuildingState *house = &place->buildings[i];
            house->plot_id = (uint32_t)i + 1U;
            house->style_seed = BuildingHash(seed ^ house->plot_id * UINT32_C(131));
            uint32_t age = house->style_seed % 30U + 2U;
            house->built_day = -(int32_t)age * 364;
            house->health = 85 + (int32_t)(house->style_seed % 16U);
            house->roof_health = 80 + (int32_t)(house->style_seed % 21U);
            house->upkeep = 60 + (int32_t)(house->style_seed % 41U);
            /* A fixed mix, with seeded details, makes each street read at a glance. */
            if (i % 5 == 1) {
                house->built_day = sim->current_day;
                house->health = house->roof_health = house->upkeep = 100;
            } else if (i % 5 == 3) {
                house->health = 55 + (int32_t)(house->style_seed % 16U);
                house->roof_health = 45 + (int32_t)(house->style_seed % 21U);
                house->upkeep = 20 + (int32_t)(house->style_seed % 21U);
            }
            house->last_change_day = sim->current_day;
        }
        ReconcileFire(place, place->last_fire_day > 0 ? place->last_fire_day : sim->current_day, false);
    }
}

void CcSettlementBuildingsSync(CcSim *sim, CcSettlement *place)
{
    if (sim == NULL || place == NULL || sim->schema_version < CC_BUILDING_SCHEMA_VERSION ||
        place->building_count <= 0 || place->building_count > CC_SETTLEMENT_BUILDING_CAPACITY) return;
    ReconcileFire(place, sim->current_day, false);
}

void CcSettlementBuildingsSetFire(CcSim *sim, CcSettlement *place, int32_t damage)
{
    if (sim == NULL || place == NULL) return;
    CcSettlementBuildingsSync(sim, place);
    place->fire_damage = ClampPercent(damage);
    if (sim->schema_version >= CC_BUILDING_SCHEMA_VERSION && place->building_count > 0 &&
        place->building_count <= CC_SETTLEMENT_BUILDING_CAPACITY)
        ReconcileFire(place, sim->current_day, true);
}

bool CcBuildingsValidate(const CcSim *sim, char *error, size_t capacity)
{
    if (sim == NULL) return false;
    if (sim->schema_version < CC_BUILDING_SCHEMA_VERSION) return true;
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        const CcSettlement *place = &sim->settlements[town];
        bool valid = place->blueprint_version == CC_BUILDING_BLUEPRINT_VERSION &&
            place->building_count == CcSettlementBlueprintBuildingCount(place->blueprint_id) &&
            place->building_count > 0 && place->building_count <= CC_SETTLEMENT_BUILDING_CAPACITY;
        uint32_t plots = 0U;
        for (int32_t i = 0; valid && i < place->building_count; ++i) {
            const CcBuildingState *house = &place->buildings[i];
            valid = house->plot_id > 0U && house->plot_id <= (uint32_t)place->building_count;
            if (!valid) break;
            uint32_t bit = UINT32_C(1) << (house->plot_id - 1U);
            valid = (plots & bit) == 0U && house->health >= 0 && house->health <= 100 &&
                house->roof_health >= 0 && house->roof_health <= 100 &&
                house->upkeep >= 0 && house->upkeep <= 100 &&
                house->fire_damage >= 0 && house->fire_damage <= 100 &&
                house->repair_progress >= 0 && house->repair_progress <= 100 &&
                house->built_day >= -1000000 && house->built_day <= sim->current_day &&
                house->last_change_day >= 0 && house->last_change_day <= sim->current_day &&
                house->last_repair_day >= 0 && house->last_repair_day <= house->last_change_day &&
                (house->repair_progress == 0 || house->last_repair_day > 0) &&
                (house->repair_progress != 100 || house->fire_damage == 0);
            plots |= bit;
        }
        if (!valid) {
            if (error != NULL && capacity > 0U)
                (void)snprintf(error, capacity, "A settlement building record is invalid.");
            return false;
        }
    }
    return true;
}
