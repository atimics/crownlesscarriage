#ifndef CC_PRODUCTION_H
#define CC_PRODUCTION_H

#include "sim/cc_sim.h"

#define CC_RECIPE_INPUTS 3

typedef struct {
    CcGood good;
    int32_t units;
    int32_t reserve;
} CcRecipeInput;

typedef struct {
    CcGood output;
    int32_t output_units;
    /* A final partial output still pays one full batch of inputs and work. */
    bool allow_partial_output;
    bool work_only;
    int32_t input_count;
    CcRecipeInput inputs[CC_RECIPE_INPUTS];
    int32_t work_per_batch;
    int32_t minimum_condition;
    int32_t tools_required;
    int32_t hunger_soft_limit;
    int32_t hunger_hard_limit;
    int32_t hunger_soft_percent;
    int32_t hunger_hard_percent;
} CcProductionRecipe;

/* All stock belongs to this one store at this actual location. Callers supply
   a site store or a co-located town store. Ownership alone never finds stock. */
typedef struct {
    CcId producer_id;
    CcId storage_id;
    CcId location_id;
    int32_t *stock;
    int32_t capacity;
    int32_t output_limit;
    int32_t work_available;
    int32_t condition;
    int32_t hunger;
    bool enabled;
} CcProductionContext;

typedef enum {
    CC_PRODUCTION_READY,
    CC_PRODUCTION_INVALID,
    CC_PRODUCTION_CLOSED,
    CC_PRODUCTION_CONDITION,
    CC_PRODUCTION_CAPACITY,
    CC_PRODUCTION_OUTPUT_FULL,
    CC_PRODUCTION_WORK,
    CC_PRODUCTION_TOOLS,
    CC_PRODUCTION_INPUT,
    CC_PRODUCTION_GATE_COUNT
} CcProductionGate;

typedef struct {
    CcId producer_id;
    CcId storage_id;
    CcId location_id;
    CcProductionGate gate;
    CcGood blocked_good;
    int32_t batches;
    int32_t output;
    int32_t inputs[CC_RECIPE_INPUTS];
    int32_t work;
} CcProductionReceipt;

CcProductionReceipt CcProductionPlan(const CcProductionRecipe *recipe,
                                     const CcProductionContext *context);
/* Plans from current stock and applies exactly that act. Scheduling and event
   aggregation belong to the caller. Receipts are observations, not commands. */
CcProductionReceipt CcProductionRun(const CcProductionRecipe *recipe,
                                    const CcProductionContext *context);
typedef struct {
    CcId site_id;
    uint64_t input[CC_GOOD_COUNT];
    uint64_t output[CC_GOOD_COUNT];
    uint64_t work;
    uint64_t route_repair;
    uint64_t gates[CC_PRODUCTION_GATE_COUNT];
} CcSiteProductionAccounting;

typedef struct {
    CcSiteProductionAccounting sites[CC_MAX_ROAD_SITES];
} CcRoadProductionAccounting;

bool CcRoadSiteRecipe(const CcRoadSite *site, CcProductionRecipe *recipe);
CcProductionReceipt CcSimPlanRoadSite(const CcSim *sim, const CcRoadSite *site);
void CcSimAdvanceDaysWithProductionAccounting(CcSim *sim, int32_t days,
    CcNutritionAccounting *nutrition, CcSmithyAccounting *smithy,
    CcRoadProductionAccounting *sites);
typedef enum {
    CC_SITE_FREIGHT_NONE, CC_SITE_FREIGHT_SUPPLY, CC_SITE_FREIGHT_PICKUP
} CcSiteFreightKind;
typedef enum {
    CC_SITE_FREIGHT_READY, CC_SITE_FREIGHT_CARRIAGE_REQUIRED,
    CC_SITE_FREIGHT_SITE_REQUIRED, CC_SITE_FREIGHT_LOCAL_TOWN_REQUIRED,
    CC_SITE_FREIGHT_ROUTE_REQUIRED, CC_SITE_FREIGHT_CAPACITY_REQUIRED,
    CC_SITE_FREIGHT_GOODS_REQUIRED
} CcSiteFreightGate;
typedef struct {
    CcSiteFreightKind kind;
    CcSiteFreightGate gate;
    CcId carriage_id, site_id, town_id, route_id;
    CcGood good;
    int32_t quantity, travel_days, return_days;
} CcSiteFreightPlan;
/* Engine preview from actual stores. Re-plan before each future loading act.
   Pickup quantity describes the return load; road capacity is a current snapshot. */
CcSiteFreightPlan CcSimPlanSiteFreight(const CcSim *sim, CcId carriage_id, CcId site_id);
#endif
