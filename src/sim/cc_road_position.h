#ifndef CROWNLESS_ROAD_POSITION_H
#define CROWNLESS_ROAD_POSITION_H

#include "sim/cc_sim.h"

/* Stable authored identities for the first physical road district. */
#define CC_PILOT_ROAD_JUNCTION_ID UINT64_C(0x7200032300000001)
#define CC_PILOT_ROAD_CHECKPOINT_ID UINT64_C(0x7200032300000002)
#define CC_PILOT_ROAD_ORIGIN_SEGMENT_ID UINT64_C(0x7300032300000001)
#define CC_PILOT_ROAD_DESTINATION_SEGMENT_ID UINT64_C(0x7300032300000002)
#define CC_PILOT_ROAD_MILL_SEGMENT_ID UINT64_C(0x7300032300000003)

#define CC_PILOT_ROAD_UNITS_PER_SPUR_UNIT INT32_C(1000)
#define CC_ROAD_GEOMETRY_UNITS_PER_WORLD_UNIT INT32_C(1000)
#define CC_ROAD_GEOMETRY_SAMPLE_COUNT 33
#define CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE 1
#define CC_ROAD_GEOMETRY_TO_JUNCTION_SAMPLE 31

typedef struct CcRoadGeometryPoint {
    int32_t x_units;
    int32_t z_units;
} CcRoadGeometryPoint;

typedef struct CcRoadGeometry {
    CcId route_id;
    CcId from_id;
    CcId to_id;
    CcRoadGeometryPoint control;
    CcRoadGeometryPoint samples[CC_ROAD_GEOMETRY_SAMPLE_COUNT];
    int32_t full_length_units;
    int32_t journey_length_units;
} CcRoadGeometry;

typedef enum CcRoadDirection {
    CC_ROAD_DIRECTION_NONE = 0,
    CC_ROAD_DIRECTION_FORWARD = 1,
    CC_ROAD_DIRECTION_REVERSE = -1
} CcRoadDirection;

typedef enum CcRoadAnchorKind {
    CC_ROAD_ANCHOR_NONE = 0,
    CC_ROAD_ANCHOR_SETTLEMENT,
    CC_ROAD_ANCHOR_JUNCTION,
    CC_ROAD_ANCHOR_SITE,
    CC_ROAD_ANCHOR_CHECKPOINT
} CcRoadAnchorKind;

typedef struct CcPilotRoadTopology {
    CcId route_id;
    CcId origin_id;
    CcId destination_id;
    CcId mill_site_id;
    CcId junction_id;
    CcId checkpoint_id;
    CcId origin_segment_id;
    CcId destination_segment_id;
    CcId mill_segment_id;
    int32_t main_length_units;
    int32_t origin_to_junction_units;
    int32_t junction_to_destination_units;
    int32_t mill_spur_length_units;
    int32_t checkpoint_distance_units;
    int32_t junction_progress_milli;
    int32_t checkpoint_progress_milli;
} CcPilotRoadTopology;

typedef struct CcRoadLegPreview {
    CcId journey_goal_id;
    CcId anchor_id;
    CcId segment_id;
    CcId destination_anchor_id;
    CcId next_named_place_id;
    CcRoadAnchorKind destination_kind;
    CcRoadDirection direction;
    int32_t length_units;
    int32_t travel_subticks;
    uint32_t journey_revision;
    uint64_t decision_token;
} CcRoadLegPreview;

bool CcPilotRoadTopologyBuild(const CcSim *sim,
                              CcPilotRoadTopology *topology);
bool CcPilotRoadTopologyBuildWithLength(const CcSim *sim,
                                        int32_t main_length_units,
                                        CcPilotRoadTopology *topology);
bool CcRoadGeometryBuild(const CcSim *sim, CcId route_id,
                         CcRoadGeometry *geometry);
int32_t CcRoadGeometryBuildAll(const CcSim *sim,
                               CcRoadGeometry *geometries,
                               int32_t capacity);
bool CcRoadGeometryKnownFixtures(void);
int32_t CcRoadScaleDistance(int32_t total_units, int32_t progress_milli);
int32_t CcRoadProgressMilli(int32_t travelled_units,
                            int32_t total_units);
int32_t CcRoadTravelSubticks(int32_t leg_units,
                             int32_t route_total_subticks,
                             int32_t route_length_units);
uint64_t CcRoadPreviewToken(CcId journey_id, CcId journey_goal_id,
                            CcId anchor_id, int32_t coordinate_units,
                            int32_t distance_travelled,
                            int32_t distance_remaining,
                            uint32_t journey_revision, CcId segment_id,
                            CcRoadDirection direction);
bool CcRoadBeginPilotJourney(CcSim *sim, CcId journey_id);
int32_t CcRoadNextLegPreviews(const CcSim *sim,
                              CcRoadLegPreview *previews,
                              int32_t capacity);
bool CcRoadChooseNextLeg(CcSim *sim, uint64_t decision_token,
                         char *error, size_t error_capacity);
bool CcRoadAdvanceLeg(CcSim *sim, int32_t journey_subticks);
bool CcRoadSavedPositionValid(const CcSim *sim);

#endif
