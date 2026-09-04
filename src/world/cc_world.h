#ifndef CROWNLESS_WORLD_H
#define CROWNLESS_WORLD_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_WORLD_CHUNK_SIZE 32.0f
#define CC_WORLD_CHUNK_CELLS 16
#define CC_WORLD_CHUNK_SAMPLES (CC_WORLD_CHUNK_CELLS + 1)
#define CC_WORLD_CHUNK_SAMPLE_COUNT \
    (CC_WORLD_CHUNK_SAMPLES * CC_WORLD_CHUNK_SAMPLES)
#define CC_WORLD_STREAM_RADIUS 2
#define CC_WORLD_STREAM_DIAMETER (CC_WORLD_STREAM_RADIUS * 2 + 1)
#define CC_WORLD_STREAM_CAPACITY \
    (CC_WORLD_STREAM_DIAMETER * CC_WORLD_STREAM_DIAMETER)
#define CC_WORLD_ROUTE_SAMPLE_COUNT 33
#define CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE 1
#define CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE \
    (CC_WORLD_ROUTE_SAMPLE_COUNT - 2)
#define CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE 8
#define CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE \
    (CC_WORLD_ROUTE_SAMPLE_COUNT - 9)
#define CC_WORLD_SITE_COUNT 3

typedef struct CcWorldPoint {
    float x;
    float z;
} CcWorldPoint;

typedef enum CcWorldSiteKind {
    CC_WORLD_SITE_UNDERROAD = 0,
    CC_WORLD_SITE_GOBLIN_TRAIL,
    CC_WORLD_SITE_DRAGON_ROOST
} CcWorldSiteKind;

typedef struct CcWorldSettlementPlacement {
    CcId settlement_id;
    CcSettlementFunction function;
    CcWorldPoint center;
    CcWorldPoint gate;
    CcWorldPoint junction;
    float plateau_height;
    float radius;
    float profile_scale;
    float entrance_heading_yaw;
    uint32_t seed;
} CcWorldSettlementPlacement;

typedef struct CcWorldRoutePlacement {
    CcId route_id;
    CcId from_id;
    CcId to_id;
    CcWorldPoint control;
    CcWorldPoint samples[CC_WORLD_ROUTE_SAMPLE_COUNT];
    uint32_t seed;
} CcWorldRoutePlacement;

typedef struct CcWorldSitePlacement {
    CcWorldSiteKind kind;
    CcId settlement_id;
    CcWorldPoint position;
    uint32_t seed;
} CcWorldSitePlacement;

typedef struct CcWorldManifest {
    uint32_t world_seed;
    uint32_t generator_version;
    float minimum_x;
    float minimum_z;
    float maximum_x;
    float maximum_z;
    CcWorldSettlementPlacement settlements[CC_MAX_SETTLEMENTS];
    int32_t settlement_count;
    CcWorldRoutePlacement routes[CC_MAX_ROUTES];
    int32_t route_count;
    CcWorldSitePlacement sites[CC_WORLD_SITE_COUNT];
    int32_t site_count;
} CcWorldManifest;

typedef enum CcWorldSurfaceKind {
    CC_WORLD_SURFACE_WILDERNESS = 0,
    CC_WORLD_SURFACE_ROAD,
    CC_WORLD_SURFACE_SETTLEMENT
} CcWorldSurfaceKind;

typedef enum CcWorldChunkState {
    CC_WORLD_CHUNK_EMPTY = 0,
    CC_WORLD_CHUNK_PENDING,
    CC_WORLD_CHUNK_READY
} CcWorldChunkState;

typedef struct CcWorldChunk {
    int32_t x;
    int32_t z;
    CcWorldChunkState state;
    uint64_t last_touched;
    uint64_t generation;
    float heights[CC_WORLD_CHUNK_SAMPLE_COUNT];
} CcWorldChunk;

typedef struct CcWorldStream {
    CcWorldManifest manifest;
    CcWorldChunk chunks[CC_WORLD_STREAM_CAPACITY];
    uint64_t tick;
    uint64_t next_generation;
    uint64_t generated_chunks;
    uint64_t evicted_chunks;
    int32_t focus_chunk_x;
    int32_t focus_chunk_z;
} CcWorldStream;

bool CcWorldManifestBuild(CcWorldManifest *manifest, const CcSim *sim);
bool CcWorldManifestContains(const CcWorldManifest *manifest, float x, float z);
const CcWorldSettlementPlacement *CcWorldSettlementPlacementForId(
    const CcWorldManifest *manifest, CcId settlement_id);
const CcWorldRoutePlacement *CcWorldRoutePlacementForId(
    const CcWorldManifest *manifest, CcId route_id);
const CcWorldSitePlacement *CcWorldSitePlacementForKind(
    const CcWorldManifest *manifest, CcWorldSiteKind kind);
const CcWorldSettlementPlacement *CcWorldNearestSettlement(
    const CcWorldManifest *manifest, float x, float z, float *distance);
CcWorldPoint CcWorldRoutePoint(const CcWorldRoutePlacement *route, float amount);
float CcWorldRouteJourneyAmount(const CcWorldRoutePlacement *route,
                                CcId origin_id, float progress);
bool CcWorldRoutePose(const CcWorldRoutePlacement *route, CcId origin_id,
                      float journey_amount, CcWorldPoint *position,
                      float *heading_yaw);
float CcWorldRouteLength(const CcWorldRoutePlacement *route);
float CcWorldRouteSampleAmount(const CcWorldRoutePlacement *route,
                               int32_t sample_index);
CcWorldPoint CcWorldSettlementLocalPoint(
    const CcWorldManifest *manifest, CcId settlement_id,
    float local_x, float local_z);
CcWorldPoint CcWorldSettlementFeaturePoint(
    const CcWorldManifest *manifest, CcId settlement_id,
    float offset_x, float offset_z);
float CcWorldTerrainHeight(const CcWorldManifest *manifest, float x, float z);
CcWorldSurfaceKind CcWorldSurfaceAt(
    const CcWorldManifest *manifest, float x, float z);

bool CcWorldStreamInit(CcWorldStream *stream, const CcSim *sim);
void CcWorldStreamUpdate(CcWorldStream *stream, float focus_x, float focus_z,
                         int32_t generation_budget);
const CcWorldChunk *CcWorldStreamChunkAt(
    const CcWorldStream *stream, int32_t chunk_x, int32_t chunk_z);
float CcWorldStreamHeightAt(const CcWorldStream *stream, float x, float z);
int32_t CcWorldStreamResidentCount(const CcWorldStream *stream);
int32_t CcWorldStreamPendingCount(const CcWorldStream *stream);
size_t CcWorldStreamResidentBytes(const CcWorldStream *stream);

#endif
