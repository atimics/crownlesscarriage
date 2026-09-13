#ifndef CROWNLESS_ARCHIVE_RELOCATION_H
#define CROWNLESS_ARCHIVE_RELOCATION_H
#include "sim/cc_sim.h"

#define CC_ARCHIVE_MOVE_BOOK_CAPACITY 4
#define CC_ARCHIVE_SEAT_FAILURE_DAYS 365

typedef enum CcArchiveRelocationGate {
    CC_ARCHIVE_MOVE_READY,
    CC_ARCHIVE_MOVE_SEAT,
    CC_ARCHIVE_MOVE_HEALTHY,
    CC_ARCHIVE_MOVE_WAIT,
    CC_ARCHIVE_MOVE_RECRUITMENT,
    CC_ARCHIVE_MOVE_DESTINATION,
    CC_ARCHIVE_MOVE_BOOKS,
    CC_ARCHIVE_MOVE_SPONSOR,
    CC_ARCHIVE_MOVE_CARRIAGE,
    CC_ARCHIVE_MOVE_ROUTE,
    CC_ARCHIVE_MOVE_FOOD,
    CC_ARCHIVE_MOVE_FUNDS,
    CC_ARCHIVE_MOVE_BUSY
} CcArchiveRelocationGate;

typedef struct CcArchiveRelocationPlan {
    CcArchiveRelocationGate gate;
    CcId origin_id, destination_id, sponsor_id, funding_kingdom_id;
    CcId carriage_id, first_route_id, first_hop_id;
    CcId book_ids[CC_ARCHIVE_MOVE_BOOK_CAPACITY];
    int32_t book_count, eligible_books, destination_score;
    int32_t first_leg_days, first_leg_wheat, route_danger;
    CcMoney first_leg_toll;
    bool rival_foundation;
} CcArchiveRelocationPlan;

/* Quote the first book convoy after sustained seat failure. A saved order must
   recheck this quote before spending funds, reserving cargo, or departing. */
CcArchiveRelocationPlan CcSimArchiveRelocationPlan(const CcSim *sim);
const char *CcArchiveRelocationGateName(CcArchiveRelocationGate gate);
bool CcSimReserveArchiveConvoy(CcSim *sim);
/* Weekly funded relocation and ended-order refunds, with named world events. */
bool CcSimAutoArchiveConvoy(CcSim *sim);
bool CcSimCancelArchiveConvoy(CcSim *sim);
bool CcSimArchiveConvoyValid(const CcSim *sim);
typedef enum CcArchiveConvoyStep {
    CC_ARCHIVE_CONVOY_WAIT,
    CC_ARCHIVE_CONVOY_DEPARTED,
    CC_ARCHIVE_CONVOY_BLOCKED,
    CC_ARCHIVE_CONVOY_ARRIVED,
    CC_ARCHIVE_CONVOY_LOST,
    CC_ARCHIVE_CONVOY_COMPLETED
} CcArchiveConvoyStep;
bool CcSimArchiveConvoyHoldsBook(const CcSim *sim, CcId book_id);
bool CcSimArchiveConvoyCarriesBook(const CcSim *sim, CcId book_id);
CcArchiveConvoyStep CcSimAdvanceArchiveConvoy(CcSim *sim, uint32_t road_roll);
#endif
