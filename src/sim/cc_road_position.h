#ifndef CROWNLESS_ROAD_POSITION_H
#define CROWNLESS_ROAD_POSITION_H

#include "sim/cc_sim.h"

/* Stable authored identities for the first physical road district. */
#define CC_PILOT_ROAD_JUNCTION_ID UINT64_C(0x7200032300000001)
#define CC_PILOT_ROAD_CHECKPOINT_ID UINT64_C(0x7200032300000002)
#define CC_PILOT_ROAD_ORIGIN_SEGMENT_ID UINT64_C(0x7300032300000001)
#define CC_PILOT_ROAD_DESTINATION_SEGMENT_ID UINT64_C(0x7300032300000002)
#define CC_PILOT_ROAD_MILL_SEGMENT_ID UINT64_C(0x7300032300000003)

#define CC_PILOT_ROAD_MAIN_LENGTH_UNITS INT32_C(52000)
#define CC_PILOT_ROAD_UNITS_PER_SPUR_UNIT INT32_C(1000)

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
int32_t CcRoadScaleDistance(int32_t total_units, int32_t progress_milli);
int32_t CcRoadProgressMilli(int32_t travelled_units,
                            int32_t total_units);
int32_t CcRoadTravelSubticks(int32_t leg_units,
                             int32_t route_total_subticks,
                             int32_t route_length_units);
uint64_t CcRoadPreviewToken(CcId journey_goal_id, CcId anchor_id,
                            uint32_t journey_revision, CcId segment_id,
                            CcRoadDirection direction);

#endif
