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

/* Authored plot centres in centimetres. These shared descriptors belong to the
   settlement definition; the client checks its visible plots against them. */
static const CcBuildingPlotPosition BLUEPRINT_PLOTS[6][CC_SETTLEMENT_BUILDING_CAPACITY] = {
    {{1,2250,1925},{2,3310,2025},{3,5000,2100},{4,2325,3825},{5,3225,4150},{6,5950,4510},
     {7,8675,2500},{8,5700,6300},{9,1750,6100},{10,2150,5050},{11,7700,4600},{12,7650,6200}},
    {{1,2400,2000},{2,3470,1925},{3,5000,2100},{4,2525,3800},{5,3325,3865},{6,5950,4550},
     {7,6050,2825},{8,7400,6100},{9,2160,5325},{10,5525,6175},{11,8625,5900}},
    {{1,2100,1650},{2,3300,1500},{3,5000,1700},{4,1700,3800},{5,2700,4150},{6,7050,5750},
     {7,6125,2575},{8,6100,1450},{9,2150,5950},{10,5400,6550}},
    {{1,2400,2000},{2,3525,1900},{3,5000,2100},{4,2500,3800},{5,3325,3900},{6,5950,4550},
     {7,6050,2700},{8,7600,6000},{9,5500,6175}},
    {{1,2400,2000},{2,3500,1900},{3,5000,2100},{4,2500,3800},{5,3325,3900},{6,5975,4550},
     {7,6050,2750},{8,7200,6100},{9,2125,5050},{10,5525,6175},{11,8600,5850},{12,8650,4550}},
    {{1,2425,2000},{2,3525,1925},{3,5000,2100},{4,2550,3775},{5,3325,3875},{6,8600,5800},
     {7,5500,6150},{8,1750,6100},{9,7200,5650}}
};

const CcBuildingPlotPosition *CcSettlementBlueprintPlotPosition(uint32_t blueprint_id,
                                                               uint32_t plot_id)
{
    if (blueprint_id < 1U || blueprint_id > 6U || plot_id < 1U ||
        plot_id > (uint32_t)CcSettlementBlueprintBuildingCount(blueprint_id)) return NULL;
    return &BLUEPRINT_PLOTS[blueprint_id - 1U][plot_id - 1U];
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

static uint32_t IntegerRoot(uint64_t square)
{
    uint64_t root = 0;
    uint64_t bit = UINT64_C(1) << 62;
    while (bit > square) bit >>= 2;
    while (bit != 0U) {
        if (square >= root + bit) {
            square -= root + bit;
            root = (root >> 1) + bit;
        } else root >>= 1;
        bit >>= 2;
    }
    return (uint32_t)root;
}

static uint32_t PlotDistance(const CcBuildingPlotPosition *point, int32_t x, int32_t z)
{
    int64_t dx = (int64_t)point->x_centimetres - x;
    int64_t dz = (int64_t)point->z_centimetres - z;
    return IntegerRoot((uint64_t)(dx * dx + dz * dz));
}

static uint32_t OldFireKey(const CcSettlement *place, uint32_t plot_id)
{
    const CcBuildingPlotPosition *point = CcSettlementBlueprintPlotPosition(place->blueprint_id, plot_id);
    if (point == NULL) return UINT32_MAX;
    uint32_t jitter = BuildingHash((uint32_t)place->id * UINT32_C(131) + plot_id - 1U) % 600U;
    return PlotDistance(point, 9600, 3600) + jitter;
}

/* A new front starts beside the approach road. Each later plot is connected to
   an already burned plot by the shortest remaining edge. A partial plot stays
   first in line, so a small additional fire extends the existing damage. */
static uint32_t FireKey(const CcSettlement *place, const CcBuildingState *house)
{
    if (house->fire_damage > 0) return 0U;
    const CcBuildingPlotPosition *point = CcSettlementBlueprintPlotPosition(place->blueprint_id, house->plot_id);
    if (point == NULL) return UINT32_MAX;
    uint32_t nearest = UINT32_MAX;
    for (int32_t i = 0; i < place->building_count; ++i) {
        const CcBuildingState *other = &place->buildings[i];
        if (other->fire_damage <= 0) continue;
        const CcBuildingPlotPosition *burning = CcSettlementBlueprintPlotPosition(place->blueprint_id, other->plot_id);
        if (burning == NULL) continue;
        uint32_t distance = PlotDistance(point, burning->x_centimetres, burning->z_centimetres);
        if (distance < nearest) nearest = distance;
    }
    return nearest == UINT32_MAX ? OldFireKey(place, house->plot_id) : nearest + 1U;
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
        } else if (FireKey(place, house) < FireKey(place, best) ||
                   (FireKey(place, house) == FireKey(place, best) && house->plot_id < best->plot_id)) {
            choice = i;
        }
    }
    return choice;
}

static void ReconcileFire(CcSettlement *place, int32_t day, bool funded)
{
    int32_t target = ClampPercent(place->fire_damage);
    int32_t previous = ClampPercent(place->building_fire_level);
    if (target == previous) return;
    int32_t total = TotalFire(place);
    int32_t difference = 0;
    if (target < previous)
        difference = -(total * (previous - target) + previous - 1) / previous;
    if (target > previous)
        difference = (100 * place->building_count - total) * (target - previous) / (100 - previous);
    if (target == 0) difference = -total;
    if (target == 100) difference = 100 * place->building_count - total;
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
    place->building_fire_level = target;
}

float CcSettlementBuildingBurn(const CcSettlement *place, uint32_t plot_id)
{
    if (place == NULL || place->building_count < 1 ||
        place->building_count > CC_SETTLEMENT_BUILDING_CAPACITY) return 0.0f;
    const CcBuildingState *house = CcSettlementBuilding(place, plot_id);
    if (house == NULL) return 0.0f;
    /* Capture fixtures and legacy callers may still set the town total. Resolve
       that pending change on a copy, so every view remains a pure read. */
    if (place->building_fire_level != ClampPercent(place->fire_damage)) {
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
        place->building_fire_level = 0;
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

void CcBuildingsMigrate(CcSim *sim)
{
    if (sim == NULL) return;
    int32_t damage[CC_MAX_SETTLEMENTS] = {0};
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        damage[town] = sim->settlements[town].fire_damage;
        sim->settlements[town].fire_damage = 0;
    }
    CcBuildingsInitialize(sim);
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        CcSettlement *place = &sim->settlements[town];
        place->fire_damage = place->building_fire_level = damage[town];
        /* Thornford's first nine plot IDs are the original saved-town scene. */
        int32_t old_count = place->blueprint_id == CC_BLUEPRINT_THORNFORD ? 9 : place->building_count;
        for (int32_t i = 0; i < old_count; ++i) {
            CcBuildingState *house = &place->buildings[i];
            uint32_t key = OldFireKey(place, house->plot_id);
            int32_t rank = 0;
            for (uint32_t other = 1U; other <= (uint32_t)old_count; ++other) {
                uint32_t other_key = OldFireKey(place, other);
                if (other_key < key || (other_key == key && other < house->plot_id)) ++rank;
            }
            int32_t burn = (damage[town] * old_count * 2 - (rank * 2 + 1) * 80) * 100 / (old_count * 50);
            house->fire_damage = ClampPercent(burn);
            house->health = ClampPercent(house->health - house->fire_damage);
            house->roof_health = ClampPercent(house->roof_health - house->fire_damage);
            if (house->fire_damage > 0) {
                house->last_change_day = place->last_fire_day;
                if (house->built_day >= place->last_fire_day) house->built_day = -364;
            }
        }
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
    if (damage > place->fire_damage) place->last_fire_day = sim->current_day;
    place->fire_damage = ClampPercent(damage);
    if (sim->schema_version >= CC_BUILDING_SCHEMA_VERSION && place->building_count > 0 &&
        place->building_count <= CC_SETTLEMENT_BUILDING_CAPACITY)
        ReconcileFire(place, sim->current_day, false);
}

void CcSettlementBuildingsRecordFundedRepair(CcSim *sim, CcSettlement *place, int32_t damage)
{
    if (sim == NULL || place == NULL) return;
    CcSettlementBuildingsSync(sim, place);
    if (damage >= place->fire_damage) return;
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
            place->building_fire_level >= 0 && place->building_fire_level <= 100 &&
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
