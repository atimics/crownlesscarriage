#ifndef CROWNLESS_ROAD_NEWS_H
#define CROWNLESS_ROAD_NEWS_H

#include "sim/cc_return.h"

/* The Return, milestone 4: news on the road (docs/design/the-return.md).

   News travels at carriage speed. While the company rides a leg, it can meet
   news about a town it has seen before and whose change it does not know:

   - at the first third of the leg, a traveller on the same road who holds
     the story tells it (told);
   - near the destination, a notice at the milestone for a public fact: a
     new ruler, a new crown, a famine, or a bandit camp (read);
   - on the last stretch, smoke over a freshly burned destination
     (witnessed). Smoke is a sight, not an encounter; when it is already in
     view at the milestone, it takes the notice's place.

   So a leg meets at most two people or notices, plus the smoke of a fire,
   and a town holds at most CC_RETURN_ROAD_NEWS pieces until the company
   leaves it again. Each piece is
   written into the town's CcTownSeen.road_news by a deterministic sim step
   (CcRoadNewsAdvance, run from the journey tick), so the digest marks the
   change and the gate voice moves on. Everything else here is pure. */

/* Route progress, in thousandths of the leg, where each kind of news waits.
   The smoke is looked for at the milestone and twice more after it. */
#define CC_ROAD_NEWS_TRAVELLER_MILLI 350
#define CC_ROAD_NEWS_APPROACH_MILLI 700
#define CC_ROAD_NEWS_SMOKE_MILLI_1 800
#define CC_ROAD_NEWS_SMOKE_MILLI_2 900
/* Smoke still rises this many days after a fire. */
#define CC_ROAD_NEWS_SMOKE_DAYS 10
/* How long the travel view shows a piece of news, in world ticks. */
#define CC_ROAD_NEWS_SHOW_TICKS (CC_WORLD_TICKS_PER_SECOND * 12)

typedef enum CcRoadNewsTrigger {
    CC_ROAD_NEWS_AT_TRAVELLER = 0, /* a third of the way: someone on the road */
    CC_ROAD_NEWS_AT_APPROACH,      /* the milestone: smoke, else a notice */
    CC_ROAD_NEWS_AT_SMOKE          /* the last stretch: smoke only */
} CcRoadNewsTrigger;

/* What the company would meet at this trigger on the current leg, or false
   when there is nothing new worth meeting. Pure. `town` receives the town
   the news is about. */
bool CcRoadNewsFind(const CcSim *sim, CcRoadNewsTrigger trigger,
                    CcRoadNews *news, CcId *town);

/* The journey tick calls this with the route progress before and after one
   step (in journey subticks). When a trigger point is crossed, the news met
   there is recorded. Only schema 123 and later. */
void CcRoadNewsAdvance(CcSim *sim, int32_t progress_before,
                       int32_t progress_after);

/* Record one piece of news; false when the town is unknown or full. The
   told channel also marks the traveller's story as told to the company. */
bool CcRoadNewsRecord(CcSim *sim, CcId town, const CcRoadNews *news);

/* The newest piece of news met on the current journey within the last
   CC_ROAD_NEWS_SHOW_TICKS, for the travel view. Pure. */
const CcRoadNews *CcRoadNewsLatest(const CcSim *sim, CcId *town);

/* Whether smoke rises over the destination of the current leg: a fire in
   the last CC_ROAD_NEWS_SMOKE_DAYS. Pure; the travel view draws it. */
bool CcRoadNewsSmokeAhead(const CcSim *sim, CcId *town, int32_t *fire_damage);

const char *CcRoadNewsChannelName(CcRoadNewsChannel channel);

#endif
