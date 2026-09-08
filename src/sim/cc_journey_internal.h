#ifndef CROWNLESS_JOURNEY_INTERNAL_H
#define CROWNLESS_JOURNEY_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared by journey execution, validation, and road-house generation. */
int32_t CcJourneyPaceRate(CcJourneyPace pace);
int32_t CcJourneyCarriageSpeedForPace(int32_t total_subticks, CcJourneyPace pace);
uint32_t CcJourneyRoadHouseSeed(const CcSim *sim, CcId route_id);
const char *CcJourneyGeneratedRoadHouseName(const CcSim *sim, CcId route_id);

/* The simulation owns event retention and knowledge gained from each event. */
typedef CcEvent *(*CcJourneyRecordEvent)(CcSim *sim, CcEventKind kind,
    CcId subject, CcId location, CcId parent, int32_t magnitude, const char *text);

bool CcJourneyApplyCommand(CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity, CcJourneyRecordEvent record_event);
void CcJourneyApplyWatchStrain(CcSim *sim);

/* Core-owned services retain random, event, gossip, and discovery ordering.
 * The command dispatcher supplies this complete, immutable table. */
typedef struct {
    CcJourneyRecordEvent record_event;
    uint32_t (*next_random)(CcSim *sim);
    CcCourier *(*courier)(CcSim *sim, CcId id);
    CcKingdom *(*kingdom)(CcSim *sim, CcId id);
    void (*exchange_gossip)(CcSim *sim, CcId carrier, CcId place,
                            const char *speaker);
    CcId (*latest_local_cause)(const CcSim *sim, CcId location);
    void (*reveal_settlement_roads)(CcSim *sim, CcId settlement);
    void (*reveal_journey_road)(CcSim *sim);
} CcJourneyDepartureServices;

bool CcJourneyDepart(CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity,
    const CcJourneyDepartureServices *services);

/* The simulation supplies all services before dispatching an encounter. */
typedef struct {
    CcJourneyRecordEvent record_event;
    uint32_t (*next_random)(CcSim *sim);
    CcRoute *(*route)(CcSim *sim, CcId id);
    CcBanditGroup *(*bandits)(CcSim *sim, CcId route_id);
    CcTreasure *(*allocate_treasure)(CcSim *sim);
    void (*create_traffic)(CcSim *sim, const CcJourneyEncounter *journey,
                           CcId parent_event_id);
} CcJourneyEncounterServices;

bool CcJourneyResolveEncounter(CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity,
    const CcJourneyEncounterServices *services);

#endif
