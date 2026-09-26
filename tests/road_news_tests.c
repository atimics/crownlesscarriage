#include "persistence/cc_save.h"
#include "sim/cc_return.h"
#include "sim/cc_road_news.h"
#include "sim/cc_sim.h"
#include "story/cc_gate_voice.h"
#include "story/cc_road_voice.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

/* The Return, milestone 4: news on the road (docs/design/the-return.md). */

static CcSim sim, other, restored;
static char error[256];

static void Check(bool ok)
{
    if (!ok) (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(ok);
}

static CcId TownByName(const CcSim *world, const char *name)
{
    for (int32_t i = 0; i < world->settlement_count; ++i)
        if (strcmp(world->settlements[i].name, name) == 0)
            return world->settlements[i].id;
    CC_CHECK(false);
    return 0U;
}

static CcSettlement *Town(CcSim *world, CcId id)
{
    for (int32_t i = 0; i < world->settlement_count; ++i)
        if (world->settlements[i].id == id) return &world->settlements[i];
    CC_CHECK(false);
    return NULL;
}

static void Ride(CcSim *world, CcId destination)
{
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = destination};
    Check(CcSimApply(world, &travel, error, sizeof(error)));
    world->journey.ambush_pending = false;
    world->pony_company.encounter = -1;
    for (int32_t step = 0; step < 200000 && world->journey.active; ++step) {
        if (world->journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(world, CC_WORLD_TICKS_PER_SECOND);
        else
            Check(CcTestContinueJourneyPause(world, error, sizeof(error)));
    }
    CC_CHECK(!world->journey.active);
    CC_CHECK(world->player.location_id == destination);
}

static const CcReturnChange *FindKind(const CcReturnDigest *digest,
                                      CcReturnChangeKind kind)
{
    for (int32_t i = 0; i < digest->change_count; ++i)
        if (digest->changes[i].kind == kind) return &digest->changes[i];
    return NULL;
}

static int32_t NewsCount(const CcTownSeen *seen)
{
    int32_t count = 0;
    for (int32_t i = 0; i < CC_RETURN_ROAD_NEWS; ++i)
        if (seen->road_news[i].channel != CC_ROAD_NEWS_NONE) ++count;
    return count;
}

static CcGossipCarrier *CarrierFor(CcSim *world, CcId id)
{
    for (int32_t i = 0; i < CcSimGossipCarrierCapacity(world); ++i)
        if (world->gossip_carriers[i].id == id) return &world->gossip_carriers[i];
    for (int32_t i = 0; i < CcSimGossipCarrierCapacity(world); ++i) {
        if (world->gossip_carriers[i].id != 0U) continue;
        world->gossip_carriers[i] = (CcGossipCarrier){.id = id};
        return &world->gossip_carriers[i];
    }
    CC_CHECK(false);
    return NULL;
}

/* A grown, active resident of `town` other than `skip`. */
static CcCharacter *Resident(CcSim *world, CcId town, CcId skip)
{
    for (int32_t i = 0; i < world->character_count; ++i) {
        CcCharacter *person = &world->characters[i];
        if (person->id == skip || person->current_settlement_id != town ||
            !CcSimCharacterIsActive(world, person) ||
            person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            CcCharacterAgeYears(world, person) < 16 ||
            (person->death_day > 0 && person->death_day <= world->current_day)) continue;
        return person;
    }
    CC_CHECK(false);
    return NULL;
}

/* A world with no stories, where the company has seen town B, stands in
   town A, and is on the road from A to B. Time does not pass: the checks
   drive the pure finder and the recorder by hand. */
typedef struct Road {
    CcId a, b;
} Road;

static Road QuietRoad(CcSim *world)
{
    CcSimInit(world, 7U);
    world->event_count = 0;
    world->event_write_index = 0;
    memset(world->gossip, 0, sizeof(world->gossip));
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        world->gossip_carriers[i].stories = 0U;
        world->gossip_carriers[i].told_player = 0U;
    }
    Road road = {.a = world->player.location_id};
    const CcRoute *route = NULL;
    for (int32_t r = 0; r < world->route_count && route == NULL; ++r)
        if (world->routes[r].from_id == road.a || world->routes[r].to_id == road.a)
            route = &world->routes[r];
    CC_CHECK(route != NULL);
    road.b = route->from_id == road.a ? route->to_id : route->from_id;
    CcReturnRecordSeen(world, road.b);
    CC_CHECK(CcReturnLastSeen(world, road.b) != NULL);
    world->journey = (CcJourneyEncounter){
        .active = true, .phase = CC_JOURNEY_PHASE_TRAVELLING,
        .origin_id = road.a, .destination_id = road.b, .route_id = route->id,
        .departure_day = world->current_day, .total_subticks = 1000,
    };
    return road;
}

/* A gossip story about `town` in `slot`, as the sim would hold it. */
static CcId Story(CcSim *world, int32_t slot, CcId town, CcEventKind kind,
                  const char *text)
{
    CcId event = CcMakeId(CC_ENTITY_EVENT, 910000U + (uint32_t)slot);
    world->gossip[slot] = (CcGossip){
        .event_id = event, .origin_id = town, .day = world->current_day,
        .kind = kind, .settlement_mask = 1U
    };
    (void)snprintf(world->gossip[slot].text, sizeof(world->gossip[slot].text), "%s", text);
    return event;
}

static void Hold(CcSim *world, CcId holder, int32_t slot, int32_t confidence, CcId source)
{
    CcGossipCarrier *carrier = CarrierFor(world, holder);
    carrier->stories |= UINT32_C(1) << (uint32_t)slot;
    carrier->versions[slot] = (CcGossipVersion){
        .source_character_id = source, .confidence = confidence};
}

/* Nothing new, a first visit, and news the company already has: no
   encounter. */
static void CheckOnlyUnknownNewsAboutKnownTowns(void)
{
    Road road = QuietRoad(&sim);
    CcRoadNews news;
    CcId town;
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &news, &town));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_APPROACH, &news, &town));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));
    /* Finding reads the world and does not change it. */
    CC_CHECK(CcSimHash(&sim) == hash);

    /* A town the company never left is a first visit: its fire is not news
       the company can compare with anything. */
    CcTownSeen forgotten = *CcReturnLastSeen(&sim, road.b);
    memset(&sim.return_memory.towns[0], 0, sizeof(sim.return_memory.towns));
    CcSettlement *b = Town(&sim, road.b);
    b->fire_damage = 70;
    b->last_fire_day = sim.current_day;
    CC_CHECK(CcRoadNewsSmokeAhead(&sim, NULL, NULL));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_APPROACH, &news, &town));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));

    /* Seen before: the smoke is news. Told already: it is not. */
    for (int32_t t = 0; t < sim.settlement_count; ++t)
        if (sim.settlements[t].id == road.b) sim.return_memory.towns[t] = forgotten;
    CC_CHECK(CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));
    CC_CHECK(news.channel == CC_ROAD_NEWS_WITNESSED && town == road.b);
    CcId fire = Story(&sim, 1, road.b, CC_EVENT_DRAGON_RETALIATION, "The dragon burns the town.");
    CcCharacter *teller = Resident(&sim, road.a, 0U);
    Hold(&sim, teller->id, 1, 80, 0U);
    CcGossipCarrier *carrier = CarrierFor(&sim, teller->id);
    carrier->told_player = UINT32_C(1) << 1U;
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, road.b, &digest));
    const CcReturnChange *burn = FindKind(&digest, CC_RETURN_CHANGE_FIRE);
    CC_CHECK(burn != NULL && burn->evidence_event_id == fire &&
             burn->knowledge == CC_RETURN_TOLD);
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));

    /* Old smoke has blown away. */
    carrier->told_player = 0U;
    b->last_fire_day = sim.current_day - CC_ROAD_NEWS_SMOKE_DAYS - 1;
    CC_CHECK(!CcRoadNewsSmokeAhead(&sim, NULL, NULL));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));
}

/* A traveller, a notice, and smoke each mark the digest; the gate voice
   then moves on. */
static void CheckChannelsMarkTheDigest(void)
{
    Road road = QuietRoad(&sim);
    CcSettlement *b = Town(&sim, road.b);
    const CcSettlement *a = Town(&sim, road.a);

    /* Hunger in B, a story about it, held by someone on this road. */
    b->hunger = 70;
    (void)Story(&sim, 2, road.b, CC_EVENT_SHORTAGE, "");
    (void)snprintf(sim.gossip[2].text, sizeof(sim.gossip[2].text),
                   "Hunger was reported in %s.", b->name);
    CcCharacter *traveller = Resident(&sim, road.b, 0U);
    CcCharacter *stay_home = Resident(&sim, road.b, traveller->id);
    Hold(&sim, stay_home->id, 2, 90, 0U);
    CcRoadNews news;
    CcId town;
    /* Someone at home holds the story, but nobody on the road does. */
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &news, &town));
    Hold(&sim, traveller->id, 2, 64, stay_home->id);
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &news, &town));
    traveller->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    traveller->travel_destination_id = road.a;
    traveller->travel_arrival_day = sim.current_day + 40;
    CC_CHECK(CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &news, &town));
    CC_CHECK(news.channel == CC_ROAD_NEWS_TOLD && town == road.b);
    CC_CHECK(news.kind == CC_RETURN_CHANGE_HUNGER);
    CC_CHECK(news.source_id == traveller->id && news.story_slot == 2);
    CC_CHECK(news.confidence == 64);
    /* The same traveller walking the other way is on the same road. */
    CcRoadNews again;
    traveller->current_settlement_id = road.a;
    traveller->travel_destination_id = road.b;
    CC_CHECK(CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &again, &town));
    traveller->current_settlement_id = road.b;
    traveller->travel_destination_id = road.a;

    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcRoadNewsRecord(&sim, town, &news));
    CC_CHECK(CcSimHash(&sim) != hash);
    CC_CHECK(CcSimStoryTold(&sim, traveller->id, 2));
    CC_CHECK(CcReturnMemoryValidate(&sim));
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, road.b, &digest));
    const CcReturnChange *hunger = FindKind(&digest, CC_RETURN_CHANGE_HUNGER);
    CC_CHECK(hunger != NULL && hunger->knowledge == CC_RETURN_TOLD && hunger->on_road);
    CC_CHECK(hunger->source_id == traveller->id && hunger->source_confidence == 64);
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_TRAVELLER, &news, &town));

    /* The traveller's line names the town, hedges, and says who told them. */
    CcRoadVoice voice;
    CC_CHECK(CcRoadVoiceBuild(&sim, road.b, &CcReturnLastSeen(&sim, road.b)->road_news[0],
                              &voice));
    CC_CHECK(voice.channel == CC_ROAD_NEWS_TOLD);
    CC_CHECK(strcmp(voice.speaker, traveller->name) == 0);
    CC_CHECK(voice.voice.on_road && !voice.voice.remembered_face);
    CC_CHECK(strstr(voice.line, b->name) != NULL);
    (void)printf("told line: %s\n", voice.line);
    CC_CHECK(strstr(voice.line, "the town") == NULL && strstr(voice.line, " here") == NULL);
    CC_CHECK(strstr(voice.line, "You're back") == NULL);
    CC_CHECK(voice.voice.clauses[0].part == CC_GATE_PART_EVENT);
    CC_CHECK(voice.voice.clauses[0].evidence == CC_GATE_EVIDENCE_STORY);
    bool road_clause = false, source_clause = false;
    for (int32_t i = 0; i < voice.voice.clause_count; ++i) {
        if (i > 0) CC_CHECK(voice.voice.clauses[i].part >= voice.voice.clauses[i - 1].part);
        if (voice.voice.clauses[i].evidence == CC_GATE_EVIDENCE_ROAD) {
            road_clause = true;
            CC_CHECK(voice.voice.clauses[i].evidence_id == road.b);
        }
        if (voice.voice.clauses[i].evidence == CC_GATE_EVIDENCE_SOURCE) {
            source_clause = true;
            CC_CHECK(voice.voice.clauses[i].evidence_id == stay_home->id);
        }
    }
    /* They left B, and heard it from someone who stayed. */
    CC_CHECK(road_clause && source_clause);
    CcSpeech speech;
    CC_CHECK(CcRoadVoiceSpeech(&sim, &voice, &speech));
    CC_CHECK(strcmp(speech.line_id, "return.road") == 0);
    (void)printf("told: %s: \"%s\"\n", voice.speaker, voice.line);

    /* A new ruler is a public fact: a notice at the milestone. */
    CcTownSeen *seen = NULL;
    for (int32_t t = 0; t < sim.settlement_count; ++t)
        if (sim.return_memory.towns[t].settlement_id == road.b)
            seen = &sim.return_memory.towns[t];
    CC_CHECK(seen != NULL && seen->ruler_id != 0U);
    CcId ruler = seen->ruler_id;
    char ruler_name[CC_NAME_CAPACITY];
    (void)snprintf(ruler_name, sizeof(ruler_name), "%s", seen->ruler_name);
    seen->ruler_id = CcMakeId(CC_ENTITY_CHARACTER, 990001U);
    (void)snprintf(seen->ruler_name, sizeof(seen->ruler_name), "Old Queen Maud");
    (void)Story(&sim, 3, road.b, CC_EVENT_ROYAL_SUCCESSION, "");
    (void)snprintf(sim.gossip[3].text, sizeof(sim.gossip[3].text),
                   "%s takes the crown.", ruler_name);
    CC_CHECK(CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_APPROACH, &news, &town));
    CC_CHECK(news.channel == CC_ROAD_NEWS_READ && town == road.b);
    CC_CHECK(news.kind == CC_RETURN_CHANGE_NEW_RULER && news.subject_id == ruler);
    CC_CHECK(news.confidence == 100 && news.source_id == 0U);
    /* No smoke yet: the last stretch has nothing to show. */
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &again, &town));
    CC_CHECK(CcRoadNewsRecord(&sim, road.b, &news));
    CC_CHECK(CcReturnDigestBuild(&sim, road.b, &digest));
    const CcReturnChange *crown = FindKind(&digest, CC_RETURN_CHANGE_NEW_RULER);
    CC_CHECK(crown != NULL && crown->knowledge == CC_RETURN_READ && crown->on_road);
    CC_CHECK(crown->source_confidence == 100);
    CC_CHECK(CcRoadVoiceBuild(&sim, road.b, &seen->road_news[1], &voice));
    CC_CHECK(voice.channel == CC_ROAD_NEWS_READ);
    CC_CHECK(strcmp(voice.source, "a notice at the milestone") == 0);
    CC_CHECK(strstr(voice.line, ruler_name) != NULL && strstr(voice.line, b->name) != NULL);
    CC_CHECK(!CcRoadVoiceSpeech(&sim, &voice, &speech));
    (void)printf("read: %s: %s\n", voice.speaker, voice.line);

    /* Before the smoke, the gate would lead with the fire. */
    b->fire_damage = 70;
    b->last_fire_day = sim.current_day;
    other = sim;
    CcGateVoice gate;
    CC_CHECK(CcGateVoiceBegin(&other, road.b, &gate));
    CC_CHECK(gate.change.kind == CC_RETURN_CHANGE_FIRE);

    CC_CHECK(CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_SMOKE, &news, &town));
    CC_CHECK(news.channel == CC_ROAD_NEWS_WITNESSED && news.kind == CC_RETURN_CHANGE_FIRE);
    CC_CHECK(CcRoadNewsRecord(&sim, road.b, &news));
    CC_CHECK(CcRoadVoiceBuild(&sim, road.b, &seen->road_news[2], &voice));
    CC_CHECK(strstr(voice.line, "smoke") != NULL && strstr(voice.line, b->name) != NULL);
    (void)printf("witnessed: %s: %s\n", voice.speaker, voice.line);
    CC_CHECK(CcReturnDigestBuild(&sim, road.b, &digest));
    const CcReturnChange *burn = FindKind(&digest, CC_RETURN_CHANGE_FIRE);
    CC_CHECK(burn != NULL && burn->knowledge == CC_RETURN_WITNESSED && burn->on_road);

    /* The payoff: the ride changed what the gate says. Everything the road
       told is skipped; the voice speaks of something new, or nothing. */
    hash = CcSimHash(&sim);
    if (CcGateVoiceBegin(&sim, road.b, &gate)) {
        CC_CHECK(gate.change.kind != CC_RETURN_CHANGE_FIRE &&
                 gate.change.kind != CC_RETURN_CHANGE_HUNGER &&
                 gate.change.kind != CC_RETURN_CHANGE_NEW_RULER);
        CC_CHECK(gate.change.knowledge == CC_RETURN_UNKNOWN);
    }
    CC_CHECK(CcSimHash(&sim) == hash);

    /* A town holds a few pieces of road news, no more. */
    CC_CHECK(NewsCount(seen) == CC_RETURN_ROAD_NEWS);
    CC_CHECK(!CcRoadNewsRecord(&sim, road.b, &news));
    CC_CHECK(!CcRoadNewsFind(&sim, CC_ROAD_NEWS_AT_APPROACH, &news, &town));
    CC_CHECK(CcReturnMemoryValidate(&sim));
    seen->road_news[0].channel = 9;
    CC_CHECK(!CcReturnMemoryValidate(&sim));
    seen->road_news[0].channel = CC_ROAD_NEWS_TOLD;
    seen->road_news[0].source_id = 0U;
    CC_CHECK(!CcReturnMemoryValidate(&sim));
    (void)a;
}

/* The journey step records at most one traveller and one milestone per leg,
   plus the smoke; smoke already in view at the milestone takes the notice's
   place. An older schema meets nothing. */
static void CheckAdvanceTriggers(void)
{
    Road road = QuietRoad(&sim);
    CcSettlement *b = Town(&sim, road.b);
    b->hunger = 70;
    (void)Story(&sim, 2, road.b, CC_EVENT_SHORTAGE, "");
    (void)snprintf(sim.gossip[2].text, sizeof(sim.gossip[2].text),
                   "Hunger was reported in %s.", b->name);
    CcCharacter *traveller = Resident(&sim, road.b, 0U);
    Hold(&sim, traveller->id, 2, 80, 0U);
    traveller->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    traveller->travel_destination_id = road.a;
    traveller->travel_arrival_day = sim.current_day + 40;
    b->fire_damage = 70;
    b->last_fire_day = sim.current_day;

    other = sim;
    CcTestStampLegacyGoods(&other, CC_ROAD_NEWS_SCHEMA_VERSION - 1U);
    uint64_t legacy = CcSimHash(&other);
    CcRoadNewsAdvance(&other, 0, 1000);
    CC_CHECK(CcSimHash(&other) == legacy);

    /* Short of the first point, nothing. */
    CcRoadNewsAdvance(&sim, 0, 340);
    const CcTownSeen *seen = CcReturnLastSeen(&sim, road.b);
    CC_CHECK(NewsCount(seen) == 0);
    CcRoadNewsAdvance(&sim, 340, 360);
    CC_CHECK(NewsCount(seen) == 1 && seen->road_news[0].channel == CC_ROAD_NEWS_TOLD);
    /* Crossing the same point again does nothing: the point is behind. */
    CcRoadNewsAdvance(&sim, 360, 400);
    CC_CHECK(NewsCount(seen) == 1);
    CcRoadNewsAdvance(&sim, 400, 1000);
    CC_CHECK(NewsCount(seen) == 2);
    CC_CHECK(seen->road_news[1].channel == CC_ROAD_NEWS_WITNESSED);
    /* The travel view shows the newest piece of this leg. */
    CcId town = 0U;
    const CcRoadNews *latest = CcRoadNewsLatest(&sim, &town);
    CC_CHECK(latest == &seen->road_news[1] && town == road.b);
    sim.clock.tick += CC_ROAD_NEWS_SHOW_TICKS + 1U;
    CC_CHECK(CcRoadNewsLatest(&sim, &town) == NULL);
}

static void CheckCodec(void)
{
    Road road = QuietRoad(&sim);
    CcSettlement *b = Town(&sim, road.b);
    b->fire_damage = 70;
    b->last_fire_day = sim.current_day;
    CcRoadNewsAdvance(&sim, 0, 1000);
    CC_CHECK(NewsCount(CcReturnLastSeen(&sim, road.b)) == 1);
    static uint8_t bytes[8192], legacy[8192];
    size_t length = CcReturnMemoryEncode(&sim.return_memory, bytes, sizeof(bytes));
    CC_CHECK(length == CcReturnMemoryEncodedSize(&sim.return_memory));
    CcReturnMemory decoded;
    CC_CHECK(CcReturnMemoryDecode(&decoded, bytes, length));
    CC_CHECK(CcReturnMemoryHash(&decoded) == CcReturnMemoryHash(&sim.return_memory));
    CC_CHECK(memcmp(&decoded.towns[0].road_news, &sim.return_memory.towns[0].road_news,
                    sizeof(decoded.towns[0].road_news)) == 0);

    /* A schema 122 blob (version 1) has no road news and still loads. */
    const size_t town_v1 = 7U * 8U + (1U + CC_RETURN_FACES) * CC_NAME_CAPACITY +
        9U * 4U + 2U * CC_GOOD_COUNT * 4U;
    const size_t town_v2 = (length - 4U) / CC_MAX_SETTLEMENTS;
    CC_CHECK(town_v2 > town_v1);
    size_t used = 0U;
    legacy[used++] = 1U;
    legacy[used++] = 0U;
    legacy[used++] = 0U;
    legacy[used++] = 0U;
    for (int32_t t = 0; t < CC_MAX_SETTLEMENTS; ++t) {
        memcpy(legacy + used, bytes + 4U + (size_t)t * town_v2, town_v1);
        used += town_v1;
    }
    CC_CHECK(CcReturnMemoryDecode(&decoded, legacy, used));
    CC_CHECK(NewsCount(&decoded.towns[0]) == 0 && NewsCount(&decoded.towns[1]) == 0);
    for (int32_t t = 0; t < CC_MAX_SETTLEMENTS; ++t) {
        CC_CHECK(decoded.towns[t].settlement_id == sim.return_memory.towns[t].settlement_id);
        CC_CHECK(decoded.towns[t].fire_damage == sim.return_memory.towns[t].fire_damage);
    }
    CC_CHECK(!CcReturnMemoryDecode(&decoded, legacy, used - 1U));
}

/* Thornford to Silverwick and back to Gloamgate after a year. Seed 4 has a
   traveller, a famine notice, and the smoke of Gloamgate burning. */
static void ReturnToGloamgate(CcSim *world, uint32_t schema)
{
    CcSimInit(world, 4U);
    if (schema != CC_SIM_SCHEMA_VERSION) CcTestStampLegacyGoods(world, schema);
    Ride(world, TownByName(world, "Gloamgate"));
    Ride(world, TownByName(world, "Silverwick"));
    CcSimAdvanceDays(world, 365);
    Ride(world, TownByName(world, "Gloamgate"));
}

static void CheckRouteScenario(void)
{
    ReturnToGloamgate(&sim, CC_SIM_SCHEMA_VERSION);
    CcId gloamgate = TownByName(&sim, "Gloamgate");
    const CcTownSeen *seen = CcReturnLastSeen(&sim, gloamgate);
    CC_CHECK(seen != NULL);
    bool told = false, read = false, witnessed = false;
    for (int32_t i = 0; i < CC_RETURN_ROAD_NEWS; ++i) {
        const CcRoadNews *news = &seen->road_news[i];
        told |= news->channel == CC_ROAD_NEWS_TOLD;
        read |= news->channel == CC_ROAD_NEWS_READ;
        witnessed |= news->channel == CC_ROAD_NEWS_WITNESSED;
        if (news->channel == CC_ROAD_NEWS_NONE) continue;
        CC_CHECK(news->day >= seen->seen_day && news->day <= sim.current_day);
        CcRoadVoice voice;
        CC_CHECK(CcRoadVoiceBuild(&sim, gloamgate, news, &voice));
        (void)printf("seed 4, day %d, %s: %s: %s\n", news->day,
                     CcRoadNewsChannelName((CcRoadNewsChannel)news->channel),
                     voice.speaker, voice.line);
    }
    CC_CHECK(told && read && witnessed);
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, gloamgate, &digest));
    const CcReturnChange *fire = FindKind(&digest, CC_RETURN_CHANGE_FIRE);
    const CcReturnChange *hunger = FindKind(&digest, CC_RETURN_CHANGE_HUNGER);
    CC_CHECK(fire != NULL && fire->knowledge == CC_RETURN_WITNESSED);
    CC_CHECK(hunger != NULL && hunger->knowledge == CC_RETURN_READ);
    CcGateVoice gate;
    CC_CHECK(CcGateVoiceBegin(&sim, gloamgate, &gate));
    CC_CHECK(gate.change.kind != CC_RETURN_CHANGE_FIRE &&
             gate.change.kind != CC_RETURN_CHANGE_HUNGER);
    (void)printf("gate after the road: %s: \"%s\"\n", gate.speaker, gate.line);

    /* Deterministic: the same seed meets the same news. */
    ReturnToGloamgate(&other, CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimHash(&other) == CcSimHash(&sim));
    CC_CHECK(memcmp(&other.return_memory, &sim.return_memory, sizeof(sim.return_memory)) == 0);

    /* Saved and loaded, the road news is the same. */
    const char *path = "road-news.ccsave";
    Check(CcSaveWrite(path, &sim, error, sizeof(error)));
    Check(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(memcmp(&restored.return_memory, &sim.return_memory,
                    sizeof(sim.return_memory)) == 0);
    (void)remove(path);

    /* Schema 122 rides meet nothing on the road, and load without news. */
    ReturnToGloamgate(&other, CC_ROAD_NEWS_SCHEMA_VERSION - 1U);
    CC_CHECK(other.schema_version == CC_ROAD_NEWS_SCHEMA_VERSION - 1U);
    for (int32_t t = 0; t < CC_MAX_SETTLEMENTS; ++t)
        CC_CHECK(NewsCount(&other.return_memory.towns[t]) == 0);
    CC_CHECK(CcReturnLastSeen(&other, gloamgate) != NULL);
    uint64_t legacy_hash = CcSimHash(&other);
    Check(CcSaveWrite(path, &other, error, sizeof(error)));
    Check(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    Check(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(CcReturnLastSeen(&restored, gloamgate) != NULL);
    restored.schema_version = CC_ROAD_NEWS_SCHEMA_VERSION - 1U;
    CC_CHECK(CcSimHash(&restored) == legacy_hash);
    (void)remove(path);
}

/* Across seeds and waits, each leg meets at most one traveller and one
   notice or smoke, plus smoke later on, and only about towns seen before. */
static void CheckSparse(void)
{
    static const uint32_t seeds[] = {1U, 2U, 3U, 5U, 6U, 7U};
    static const int32_t waits[] = {60, 365};
    int32_t total = 0;
    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
        for (size_t w = 0; w < sizeof(waits) / sizeof(waits[0]); ++w) {
            CcSimInit(&sim, seeds[s]);
            static const char *legs[] = {"Gloamgate", "Silverwick", "Gloamgate", "Thornford"};
            for (size_t l = 0; l < sizeof(legs) / sizeof(legs[0]); ++l) {
                if (l == 2) CcSimAdvanceDays(&sim, waits[w]);
                CcReturnMemory before = sim.return_memory;
                Ride(&sim, TownByName(&sim, legs[l]));
                int32_t met = 0, smoke = 0;
                for (int32_t t = 0; t < CC_MAX_SETTLEMENTS; ++t) {
                    const CcTownSeen *seen = &sim.return_memory.towns[t];
                    for (int32_t n = 0; n < CC_RETURN_ROAD_NEWS; ++n) {
                        const CcRoadNews *news = &seen->road_news[n];
                        if (news->channel == CC_ROAD_NEWS_NONE ||
                            (before.towns[t].road_news[n].channel == news->channel &&
                             before.towns[t].road_news[n].tick == news->tick)) continue;
                        /* Only about a town the company left before. */
                        CC_CHECK(before.towns[t].settlement_id == seen->settlement_id);
                        CC_CHECK(news->day >= seen->seen_day);
                        CcRoadVoice voice;
                        CC_CHECK(CcRoadVoiceBuild(&sim, seen->settlement_id, news, &voice));
                        if (news->channel == CC_ROAD_NEWS_WITNESSED) ++smoke;
                        else ++met;
                    }
                }
                CC_CHECK(met <= 2 && smoke <= 1);
                total += met + smoke;
            }
        }
    }
    /* Sparse, not silent. */
    CC_CHECK(total > 0 && total <= 2 * (int32_t)(sizeof(seeds) / sizeof(seeds[0])) * 2);
    (void)printf("road news over %zu rides: %d pieces\n",
                 sizeof(seeds) / sizeof(seeds[0]) * 2, total);
}

int main(void)
{
    CheckOnlyUnknownNewsAboutKnownTowns();
    CheckChannelsMarkTheDigest();
    CheckAdvanceTriggers();
    CheckCodec();
    CheckRouteScenario();
    CheckSparse();
    (void)printf("road news tests passed\n");
    return 0;
}
