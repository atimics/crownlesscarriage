#include "sim/cc_road_news.h"

#include <string.h>

static int32_t SettlementSlot(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->settlement_count && i < CC_MAX_SETTLEMENTS; ++i)
        if (sim->settlements[i].id == id) return i;
    return -1;
}

static int32_t StorySlot(const CcSim *sim, CcId event_id)
{
    if (event_id == 0U) return -1;
    for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot)
        if (sim->gossip[slot].event_id == event_id) return slot;
    return -1;
}

static int32_t FreeNewsSlot(const CcTownSeen *seen)
{
    for (int32_t i = 0; i < CC_RETURN_ROAD_NEWS; ++i)
        if (seen->road_news[i].channel == CC_ROAD_NEWS_NONE) return i;
    return -1;
}

/* Changes a traveller can carry as a story. Market prices, restocked
   stalls and the like are not road news. */
static bool TellableKind(CcReturnChangeKind kind)
{
    switch (kind) {
    case CC_RETURN_CHANGE_ABANDONED:
    case CC_RETURN_CHANGE_FIRE:
    case CC_RETURN_CHANGE_REBUILT:
    case CC_RETURN_CHANGE_NEW_RULER:
    case CC_RETURN_CHANGE_NEW_KINGDOM:
    case CC_RETURN_CHANGE_FACE_DIED:
    case CC_RETURN_CHANGE_HUNGER:
    case CC_RETURN_CHANGE_LAWLESS:
    case CC_RETURN_CHANGE_BANDIT_CAMP:
    case CC_RETURN_CHANGE_DRAGON_OMEN:
    case CC_RETURN_CHANGE_SERVICE_LOST:
    case CC_RETURN_CHANGE_STALL_EMPTY:
        return true;
    default:
        return false;
    }
}

/* Public facts that someone nails to a milestone: a proclamation, a famine
   notice, a bounty. */
static bool NoticeKind(const CcReturnChange *change)
{
    switch (change->kind) {
    case CC_RETURN_CHANGE_NEW_RULER:
    case CC_RETURN_CHANGE_NEW_KINGDOM:
    case CC_RETURN_CHANGE_HUNGER:
        return true;
    case CC_RETURN_CHANGE_BANDIT_CAMP:
        return change->detail == 0; /* a camp, not a planned raid */
    default:
        return false;
    }
}

static bool OnThisRoad(const CcSim *sim, const CcCharacter *person)
{
    CcId a = sim->journey.origin_id, b = sim->journey.destination_id;
    if (!CcSimCharacterIsActive(sim, person) ||
        person->activity != CC_CHARACTER_ACTIVITY_TRAVELLING ||
        person->travel_destination_id == 0U ||
        (person->death_day > 0 && person->death_day <= sim->current_day) ||
        CcCharacterAgeYears(sim, person) < 16) return false;
    return (person->current_settlement_id == a && person->travel_destination_id == b) ||
        (person->current_settlement_id == b && person->travel_destination_id == a);
}

bool CcRoadNewsSmokeAhead(const CcSim *sim, CcId *town, int32_t *fire_damage)
{
    if (town != NULL) *town = 0U;
    if (fire_damage != NULL) *fire_damage = 0;
    if (sim == NULL || !sim->journey.active) return false;
    const CcSettlement *place = CcSimSettlement(sim, sim->journey.destination_id);
    if (place == NULL || place->fire_damage < 5 || place->last_fire_day <= 0 ||
        sim->current_day - place->last_fire_day > CC_ROAD_NEWS_SMOKE_DAYS) return false;
    if (town != NULL) *town = place->id;
    if (fire_damage != NULL) *fire_damage = place->fire_damage;
    return true;
}

static void FromChange(const CcSim *sim, const CcReturnChange *change,
                       CcRoadNewsChannel channel, CcRoadNews *news)
{
    *news = (CcRoadNews){
        .channel = (int32_t)channel,
        .kind = (int32_t)change->kind,
        .detail = change->detail,
        .confidence = 100,
        .subject_id = change->subject_id,
        .event_id = change->evidence_event_id,
        .route_id = sim->journey.route_id,
        .story_slot = -1,
        .day = sim->current_day,
        .tick = sim->clock.tick,
    };
}

/* The traveller with the best untold story about a town the company has
   seen: the highest-ranked unknown change first, then the town slot, then
   the traveller's place in the cast. */
static bool FindTraveller(const CcSim *sim, CcRoadNews *news, CcId *town)
{
    CcId smoke_town = 0U;
    (void)CcRoadNewsSmokeAhead(sim, &smoke_town, NULL);
    int32_t carriers = CcSimGossipCarrierCapacity(sim);
    int32_t best_score = -1;
    for (int32_t t = 0; t < sim->settlement_count && t < CC_MAX_SETTLEMENTS; ++t) {
        const CcTownSeen *seen = &sim->return_memory.towns[t];
        if (seen->settlement_id == 0U || FreeNewsSlot(seen) < 0) continue;
        CcReturnDigest digest;
        if (!CcReturnDigestBuild(sim, seen->settlement_id, &digest) ||
            digest.first_visit) continue;
        for (int32_t c = 0; c < digest.change_count; ++c) {
            const CcReturnChange *change = &digest.changes[c];
            if (change->knowledge != CC_RETURN_UNKNOWN ||
                !TellableKind(change->kind) || change->score <= best_score) continue;
            /* A fresh fire at the leg's end is left for the smoke. */
            if (change->kind == CC_RETURN_CHANGE_FIRE &&
                seen->settlement_id == smoke_town) continue;
            int32_t slot = StorySlot(sim, change->evidence_event_id);
            if (slot < 0) continue;
            uint32_t bit = UINT32_C(1) << (uint32_t)slot;
            for (int32_t i = 0; i < sim->character_count; ++i) {
                const CcCharacter *person = &sim->characters[i];
                if (!OnThisRoad(sim, person)) continue;
                const CcGossipCarrier *carrier = NULL;
                for (int32_t k = 0; k < carriers && carrier == NULL; ++k)
                    if (sim->gossip_carriers[k].id == person->id)
                        carrier = &sim->gossip_carriers[k];
                if (carrier == NULL || (carrier->stories & bit) == 0U ||
                    (carrier->told_player & bit) != 0U) continue;
                FromChange(sim, change, CC_ROAD_NEWS_TOLD, news);
                news->source_id = person->id;
                news->story_slot = slot;
                news->confidence = carrier->versions[slot].confidence < 0 ? 0 :
                    carrier->versions[slot].confidence > 100 ? 100 :
                    carrier->versions[slot].confidence;
                *town = seen->settlement_id;
                best_score = change->score;
                break;
            }
        }
    }
    return best_score >= 0;
}

/* Near the destination: smoke over a fresh fire, else a notice for the
   first public fact the company does not know. */
static bool FindApproach(const CcSim *sim, CcRoadNews *news, CcId *town,
                         bool notice)
{
    CcId destination = sim->journey.destination_id;
    const CcTownSeen *seen = CcReturnLastSeen(sim, destination);
    if (seen == NULL || FreeNewsSlot(seen) < 0) return false;
    bool smoke = CcRoadNewsSmokeAhead(sim, NULL, NULL);
    if (!smoke && !notice) return false;
    CcReturnDigest digest;
    if (!CcReturnDigestBuild(sim, destination, &digest) || digest.first_visit)
        return false;
    for (int32_t c = 0; smoke && c < digest.change_count; ++c) {
        const CcReturnChange *change = &digest.changes[c];
        if (change->kind != CC_RETURN_CHANGE_FIRE ||
            change->knowledge != CC_RETURN_UNKNOWN) continue;
        FromChange(sim, change, CC_ROAD_NEWS_WITNESSED, news);
        *town = destination;
        return true;
    }
    for (int32_t c = 0; notice && c < digest.change_count; ++c) {
        const CcReturnChange *change = &digest.changes[c];
        if (change->knowledge != CC_RETURN_UNKNOWN || !NoticeKind(change) ||
            change->evidence_event_id == 0U) continue;
        FromChange(sim, change, CC_ROAD_NEWS_READ, news);
        *town = destination;
        return true;
    }
    return false;
}

bool CcRoadNewsFind(const CcSim *sim, CcRoadNewsTrigger trigger,
                    CcRoadNews *news, CcId *town)
{
    CcRoadNews found = {0};
    CcId about = 0U;
    if (news != NULL) *news = (CcRoadNews){0};
    if (town != NULL) *town = 0U;
    if (sim == NULL || news == NULL || !sim->journey.active ||
        sim->schema_version < CC_ROAD_NEWS_SCHEMA_VERSION) return false;
    bool ok = trigger == CC_ROAD_NEWS_AT_TRAVELLER ?
        FindTraveller(sim, &found, &about) :
        FindApproach(sim, &found, &about, trigger == CC_ROAD_NEWS_AT_APPROACH);
    if (!ok) return false;
    *news = found;
    if (town != NULL) *town = about;
    return true;
}

bool CcRoadNewsRecord(CcSim *sim, CcId town, const CcRoadNews *news)
{
    if (sim == NULL || news == NULL || news->channel == CC_ROAD_NEWS_NONE) return false;
    int32_t t = SettlementSlot(sim, town);
    if (t < 0) return false;
    CcTownSeen *seen = &sim->return_memory.towns[t];
    int32_t slot = FreeNewsSlot(seen);
    if (seen->settlement_id != town || slot < 0) return false;
    if (news->channel == CC_ROAD_NEWS_TOLD) {
        if (news->story_slot < 0 || news->story_slot >= CC_MAX_GOSSIP) return false;
        uint32_t bit = UINT32_C(1) << (uint32_t)news->story_slot;
        CcGossipCarrier *carrier = NULL;
        for (int32_t k = 0; k < CcSimGossipCarrierCapacity(sim) && carrier == NULL; ++k)
            if (sim->gossip_carriers[k].id == news->source_id)
                carrier = &sim->gossip_carriers[k];
        if (carrier == NULL || (carrier->stories & bit) == 0U) return false;
        carrier->told_player |= bit;
    }
    seen->road_news[slot] = *news;
    return true;
}

void CcRoadNewsAdvance(CcSim *sim, int32_t progress_before, int32_t progress_after)
{
    if (sim == NULL || sim->schema_version < CC_ROAD_NEWS_SCHEMA_VERSION ||
        !sim->journey.active || sim->journey.total_subticks <= 0 ||
        progress_after <= progress_before) return;
    static const struct {
        CcRoadNewsTrigger trigger;
        int32_t milli;
    } points[] = {
        {CC_ROAD_NEWS_AT_TRAVELLER, CC_ROAD_NEWS_TRAVELLER_MILLI},
        {CC_ROAD_NEWS_AT_APPROACH, CC_ROAD_NEWS_APPROACH_MILLI},
        {CC_ROAD_NEWS_AT_SMOKE, CC_ROAD_NEWS_SMOKE_MILLI_1},
        {CC_ROAD_NEWS_AT_SMOKE, CC_ROAD_NEWS_SMOKE_MILLI_2},
    };
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        int32_t at = (int32_t)((int64_t)sim->journey.total_subticks *
                               points[i].milli / 1000);
        if (progress_before >= at || progress_after < at) continue;
        CcRoadNews news;
        CcId town = 0U;
        if (CcRoadNewsFind(sim, points[i].trigger, &news, &town))
            (void)CcRoadNewsRecord(sim, town, &news);
    }
}

const CcRoadNews *CcRoadNewsLatest(const CcSim *sim, CcId *town)
{
    if (town != NULL) *town = 0U;
    if (sim == NULL || !sim->journey.active) return NULL;
    const CcRoadNews *best = NULL;
    for (int32_t t = 0; t < sim->settlement_count && t < CC_MAX_SETTLEMENTS; ++t) {
        const CcTownSeen *seen = &sim->return_memory.towns[t];
        for (int32_t i = 0; seen->settlement_id != 0U && i < CC_RETURN_ROAD_NEWS; ++i) {
            const CcRoadNews *news = &seen->road_news[i];
            if (news->channel == CC_ROAD_NEWS_NONE ||
                news->route_id != sim->journey.route_id ||
                news->tick > sim->clock.tick ||
                sim->clock.tick - news->tick > (uint64_t)CC_ROAD_NEWS_SHOW_TICKS)
                continue;
            if (best == NULL || news->tick >= best->tick) {
                best = news;
                if (town != NULL) *town = seen->settlement_id;
            }
        }
    }
    return best;
}

const char *CcRoadNewsChannelName(CcRoadNewsChannel channel)
{
    switch (channel) {
    case CC_ROAD_NEWS_TOLD: return "told";
    case CC_ROAD_NEWS_READ: return "read";
    case CC_ROAD_NEWS_WITNESSED: return "witnessed";
    default: return "none";
    }
}
