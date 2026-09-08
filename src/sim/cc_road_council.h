#ifndef CC_ROAD_COUNCIL_H
#define CC_ROAD_COUNCIL_H
#include "sim/cc_sim.h"

enum { CC_ROAD_COUNCIL_ROWS = 6 };
typedef struct CcRoadCouncilRow {
    CcId actor_id;
    CcId situation_id;
    char name[CC_NAME_CAPACITY];
    char detail[256];
} CcRoadCouncilRow;
typedef struct CcRoadCouncil {
    CcId settlement_id, route_id;
    int32_t route_slot;
    CcRoadCouncilRow rows[CC_ROAD_COUNCIL_ROWS];
} CcRoadCouncil;

/* A view of present needs and public commissions. Reading preserves the world. */
CcRoadCouncil CcSimRoadCouncil(const CcSim *sim, CcId settlement_id);
#endif
