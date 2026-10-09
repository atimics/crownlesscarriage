#ifndef CROWNLESS_BUILDINGS_H
#define CROWNLESS_BUILDINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_BUILDING_SCHEMA_VERSION 126U
#define CC_SETTLEMENT_BUILDING_CAPACITY 12
#define CC_BUILDING_BLUEPRINT_VERSION 1U

typedef enum CcSettlementBlueprint {
    CC_BLUEPRINT_THORNFORD = 1,
    CC_BLUEPRINT_SILVERWICK,
    CC_BLUEPRINT_GLOAMGATE,
    CC_BLUEPRINT_ALDERWATCH,
    CC_BLUEPRINT_ROSESPIRE,
    CC_BLUEPRINT_HOLLOWBARROW
} CcSettlementBlueprint;

typedef struct CcBuildingState {
    uint32_t plot_id; /* Permanent within a blueprint; independent of array order. */
    uint32_t style_seed;
    int32_t health;      /* 0..100, sound structure = 100. */
    int32_t roof_health; /* 0..100, complete roof = 100. */
    int32_t upkeep;      /* 0..100, cared-for surfaces = 100. */
    int32_t fire_damage; /* 0..100, gutted = 100. */
    int32_t repair_progress; /* 0 = idle, 1..99 = funded work, 100 = finished. */
    int32_t built_day; /* Negative dates describe buildings older than the campaign. */
    int32_t last_change_day;
    int32_t last_repair_day; /* 0 until a funded repair has started. */
} CcBuildingState;

typedef struct CcBuildingPlotPosition {
    uint32_t plot_id;
    int32_t x_centimetres;
    int32_t z_centimetres;
} CcBuildingPlotPosition;

struct CcSettlement;
struct CcSim;

const CcBuildingState *CcSettlementBuilding(const struct CcSettlement *place,
                                            uint32_t plot_id);
float CcSettlementBuildingBurn(const struct CcSettlement *place, uint32_t plot_id);
uint32_t CcSettlementBlueprintForFunction(int32_t function);
int32_t CcSettlementBlueprintBuildingCount(uint32_t blueprint_id);
const CcBuildingPlotPosition *CcSettlementBlueprintPlotPosition(uint32_t blueprint_id,
                                                               uint32_t plot_id);
void CcBuildingsInitialize(struct CcSim *sim);
void CcBuildingsMigrate(struct CcSim *sim);
/* Reconcile a changed town total with saved plots, preserving their identities.
   This also supports old capture fixtures that assign town fire_damage directly. */
void CcSettlementBuildingsSync(struct CcSim *sim, struct CcSettlement *place);
void CcSettlementBuildingsSetFire(struct CcSim *sim, struct CcSettlement *place,
                                  int32_t damage);
/* Called by the town repair step after it spends Wood, Stone and Tools. */
void CcSettlementBuildingsRecordFundedRepair(struct CcSim *sim,
                                             struct CcSettlement *place, int32_t damage);
bool CcBuildingsValidate(const struct CcSim *sim, char *error, size_t capacity);

#endif
