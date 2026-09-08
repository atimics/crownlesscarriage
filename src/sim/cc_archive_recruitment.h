#ifndef CC_ARCHIVE_RECRUITMENT_H
#define CC_ARCHIVE_RECRUITMENT_H
#include "sim/cc_sim.h"

typedef enum CcArchiveRecruitmentGate {
    CC_ARCHIVE_RECRUIT_READY,
    CC_ARCHIVE_RECRUIT_UNAVAILABLE,
    CC_ARCHIVE_RECRUIT_FULL,
    CC_ARCHIVE_RECRUIT_SEAT,
    CC_ARCHIVE_RECRUIT_CANDIDATE,
    CC_ARCHIVE_RECRUIT_TRAINER,
    CC_ARCHIVE_RECRUIT_ROUTE,
    CC_ARCHIVE_RECRUIT_TRAVEL_FOOD,
    CC_ARCHIVE_RECRUIT_MATERIALS,
    CC_ARCHIVE_RECRUIT_SILENCE,
    CC_ARCHIVE_RECRUIT_FUNDS,
    CC_ARCHIVE_RECRUIT_PATRON,
    CC_ARCHIVE_RECRUIT_BUSY,
    CC_ARCHIVE_RECRUIT_CALENDAR
} CcArchiveRecruitmentGate;

typedef struct CcArchiveRecruitmentPlan {
    CcArchiveRecruitmentGate gate;
    CcId seat_id, person_id, trainer_id, origin_id, first_route_id, first_hop_id;
    CcId patron_ids[2];
    CcArchiveFundingPlan funding;
    CcMoney wages;
    int32_t training_days, trainer_days, travel_days, travel_wheat;
    int32_t wheat, paper, tools;
    int32_t first_task_wheat, first_task_paper;
    int64_t arrival_day, ready_day;
    bool training, recovery, dangerous;
} CcArchiveRecruitmentPlan;

/* Quote one hire or local apprenticeship from current people, stores and roads.
   A work order must recheck this quote before committing costs or departure. */
CcArchiveRecruitmentPlan CcSimArchiveRecruitmentPlan(const CcSim *sim);
const char *CcArchiveRecruitmentGateName(CcArchiveRecruitmentGate gate);
/* Rechecks the quote and transfers its funds and stock into a saved reservation. */
bool CcSimBeginArchiveRecruitment(CcSim *sim);
/* Return an unused reservation to its original funders and stores. */
bool CcSimCancelArchiveRecruitment(CcSim *sim);
bool CcSimArchiveRecruitmentOrderValid(const CcSim *sim);
typedef enum CcArchiveJourneyStep {
    CC_ARCHIVE_JOURNEY_WAIT,
    CC_ARCHIVE_JOURNEY_DEPARTED,
    CC_ARCHIVE_JOURNEY_STOP,
    CC_ARCHIVE_JOURNEY_ARRIVED,
    CC_ARCHIVE_JOURNEY_FAILED
} CcArchiveJourneyStep;
/* Daily progress; road_roll is used at a due road arrival, with courier risk. */
CcArchiveJourneyStep CcSimAdvanceArchiveRecruitmentJourney(CcSim *sim, uint32_t road_roll);
CcArchiveRecruitmentGate CcSimArchiveRecruitmentJourneyGate(const CcSim *sim);
typedef enum CcArchiveTrainingStep {
    CC_ARCHIVE_TRAINING_WAIT,
    CC_ARCHIVE_TRAINING_WORKED,
    CC_ARCHIVE_TRAINING_COMPLETE,
    CC_ARCHIVE_TRAINING_FAILED
} CcArchiveTrainingStep;
CcArchiveRecruitmentGate CcSimArchiveRecruitmentTrainingGate(const CcSim *sim);
CcArchiveTrainingStep CcSimAdvanceArchiveRecruitmentTraining(CcSim *sim);
#endif
