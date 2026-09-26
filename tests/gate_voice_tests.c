#include "persistence/cc_save.h"
#include "sim/cc_return.h"
#include "sim/cc_sim.h"
#include "story/cc_gate_voice.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

/* The Return, milestone 3: a resident at the gate says what changed. */

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

static int32_t RankOf(const CcReturnDigest *digest, CcReturnChangeKind kind)
{
    for (int32_t i = 0; i < digest->change_count; ++i)
        if (digest->changes[i].kind == kind) return i;
    return -1;
}

static const CcReturnChange *FirstUnknown(const CcReturnDigest *digest)
{
    for (int32_t i = 0; i < digest->change_count; ++i)
        if (digest->changes[i].knowledge == CC_RETURN_UNKNOWN) return &digest->changes[i];
    return NULL;
}

/* Every clause traces to sim state or to the speaker's gossip version. */
static void CheckEvidence(const CcSim *world, const CcGateVoice *voice)
{
    CC_CHECK(voice->clause_count > 0);
    CC_CHECK(voice->line[0] != '\0');
    const CcTownSeen *seen = CcReturnLastSeen(world, voice->settlement_id);
    CC_CHECK(seen != NULL);
    const CcGossipCarrier *carrier = CcSimGossipCarrier(world, voice->speaker_id);
    for (int32_t i = 0; i < voice->clause_count; ++i) {
        const CcGateVoiceClause *clause = &voice->clauses[i];
        CC_CHECK(clause->text[0] != '\0');
        CC_CHECK(strstr(voice->line, clause->text) != NULL);
        switch (clause->evidence) {
        case CC_GATE_EVIDENCE_STORY: {
            CC_CHECK(voice->telling == CC_GATE_VOICE_HEARD);
            CC_CHECK(clause->evidence_id == voice->change.evidence_event_id);
            CC_CHECK(voice->story_slot >= 0);
            CC_CHECK(world->gossip[voice->story_slot].event_id == clause->evidence_id);
            CC_CHECK(carrier != NULL);
            CC_CHECK((carrier->stories & (UINT32_C(1) << (uint32_t)voice->story_slot)) != 0U);
            CC_CHECK(carrier->versions[voice->story_slot].confidence == voice->confidence);
            /* Every spoken fact is a span of the speaker's own telling. */
            char telling[CC_EVENT_TEXT_CAPACITY];
            CcGossipVersion plain = carrier->versions[voice->story_slot];
            plain.court_bias = 0;
            plain.alarm = 0;
            CcGossipText(world, &world->gossip[voice->story_slot], &plain,
                         telling, sizeof(telling));
            for (int32_t f = 0; f < voice->fact_count; ++f)
                CC_CHECK(strstr(telling, voice->facts[f].value) != NULL);
            break;
        }
        case CC_GATE_EVIDENCE_SOURCE:
            CC_CHECK(voice->telling == CC_GATE_VOICE_HEARD);
            CC_CHECK(clause->evidence_id == voice->version.source_character_id);
            break;
        case CC_GATE_EVIDENCE_TOWN:
            CC_CHECK(clause->evidence_id == voice->settlement_id);
            CC_CHECK(clause->town_state);
            break;
        case CC_GATE_EVIDENCE_FACE: {
            bool remembered = false;
            for (int32_t f = 0; f < CC_RETURN_FACES; ++f)
                if (seen->face_ids[f] == voice->speaker_id) remembered = true;
            CC_CHECK(remembered && clause->evidence_id == voice->speaker_id);
            break;
        }
        default:
            CC_CHECK(false);
        }
        /* Words that read the town now must match the town now. */
        const CcSettlement *place = CcSimSettlement(world, voice->settlement_id);
        CC_CHECK(place != NULL);
        if (strstr(clause->text, "Bread's gone") != NULL)
            CC_CHECK(clause->town_state && place->stock[CC_GOOD_BREAD] == 0);
        if (strstr(clause->text, "going hungry") != NULL)
            CC_CHECK(clause->town_state && place->hunger >= 50);
        if (strstr(clause->text, "most of the town") != NULL)
            CC_CHECK(clause->town_state && place->fire_damage >= 60);
    }
    /* Gate order: greeting, what happened, why, who said so. An event always
       comes before its cause or source. */
    bool event = false;
    for (int32_t i = 0; i < voice->clause_count; ++i) {
        if (i > 0) CC_CHECK(voice->clauses[i].part >= voice->clauses[i - 1].part);
        if (voice->clauses[i].part == CC_GATE_PART_EVENT) event = true;
        if (voice->clauses[i].part >= CC_GATE_PART_CAUSE) CC_CHECK(event);
    }
    CC_CHECK(event);
    /* Residents do not name their own town to a visitor. */
    const CcSettlement *home = CcSimSettlement(world, voice->settlement_id);
    CC_CHECK(strstr(voice->line, home->name) == NULL);
}

/* Thornford to Silverwick and back to Gloamgate after a year (seed 4 burns
   Gloamgate and leaves it hungry). The ride back meets news on the road
   (milestone 4: tests/road_news_tests.c); these checks are about the gate,
   so the company forgets it, as if the ride had been quiet. */
static void ReturnToGloamgate(CcSim *world)
{
    CcSimInit(world, 4U);
    Ride(world, TownByName(world, "Gloamgate"));
    Ride(world, TownByName(world, "Silverwick"));
    CcSimAdvanceDays(world, 365);
    Ride(world, TownByName(world, "Gloamgate"));
    for (int32_t t = 0; t < CC_MAX_SETTLEMENTS; ++t)
        memset(world->return_memory.towns[t].road_news, 0,
               sizeof(world->return_memory.towns[t].road_news));
}

static void CheckRouteVoice(void)
{
    ReturnToGloamgate(&sim);
    CcId town = TownByName(&sim, "Gloamgate");
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CC_CHECK(digest.change_count > 1);
    CC_CHECK(digest.changes[0].kind == CC_RETURN_CHANGE_FIRE);

    uint64_t hash = CcSimHash(&sim);
    CcGateVoice voice = {0}, again = {0};
    CC_CHECK(CcGateVoiceBegin(&sim, town, &voice));
    /* Building the line reads the world and does not change it. */
    CC_CHECK(CcSimHash(&sim) == hash);
    CC_CHECK(voice.change.kind == CC_RETURN_CHANGE_FIRE);
    CC_CHECK(voice.telling == CC_GATE_VOICE_HEARD);
    CC_CHECK(voice.speaker_id != 0U && voice.speaker[0] != '\0');
    /* What happened comes first, then why: "Varkesh ... burned most of the
       town. It was over missing hoard money, I hear." */
    CC_CHECK(strncmp(voice.line, sim.dragon.name, strlen(sim.dragon.name)) == 0);
    CC_CHECK(strstr(voice.line, "burned most of the town.") != NULL);
    CC_CHECK(voice.clause_count == 2);
    CC_CHECK(voice.clauses[0].part == CC_GATE_PART_EVENT);
    CC_CHECK(voice.clauses[0].evidence == CC_GATE_EVIDENCE_STORY);
    CC_CHECK(voice.clauses[1].part == CC_GATE_PART_CAUSE);
    CC_CHECK(strstr(voice.clauses[1].text, "missing hoard money") != NULL);
    /* Fact selection asked who did it and chose the dragon. */
    CC_CHECK(voice.asked_role == CC_CORE_ACTOR);
    CC_CHECK(voice.chosen_fact >= 0);
    CC_CHECK(strcmp(voice.facts[voice.chosen_fact].value, sim.dragon.name) == 0);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);

    /* The same world gives the same speaker and the same words. */
    ReturnToGloamgate(&other);
    CC_CHECK(CcSimHash(&other) == hash);
    CC_CHECK(CcGateVoiceBegin(&other, town, &again));
    CC_CHECK(again.speaker_id == voice.speaker_id);
    CC_CHECK(strcmp(again.line, voice.line) == 0);

    /* Speaking marks the story as told, through a journalled command. */
    CcCommand told;
    CC_CHECK(CcGateVoiceToldCommand(&voice, &told));
    CC_CHECK(told.kind == CC_COMMAND_HEARD_STORY && told.target_id == voice.speaker_id);
    Check(CcSimApply(&sim, &told, error, sizeof(error)));
    CC_CHECK(CcSimStoryTold(&sim, voice.speaker_id, voice.story_slot));
    CC_CHECK(CcSimHash(&sim) != hash);
    CcReturnDigest after;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &after));
    int32_t fire = RankOf(&after, CC_RETURN_CHANGE_FIRE);
    CC_CHECK(fire > 0);
    CC_CHECK(after.changes[fire].knowledge == CC_RETURN_TOLD);
    CC_CHECK(after.changes[fire].source_id == voice.speaker_id);
    CC_CHECK(after.changes[fire].score < digest.changes[0].score);
    const CcReturnChange *next = FirstUnknown(&after);
    CC_CHECK(next != NULL && next->kind != CC_RETURN_CHANGE_FIRE);

    /* "Tell me more" moves on; so does a fresh voice on the same visit. */
    CC_CHECK(voice.more);
    CC_CHECK(CcGateVoiceNext(&sim, &voice));
    CC_CHECK(voice.change.kind == next->kind);
    CC_CHECK(voice.change.kind == CC_RETURN_CHANGE_HUNGER);
    /* Concrete and personal, from the town's stalls and hunger now. */
    CC_CHECK(strcmp(voice.line, "Bread's gone. People are going hungry here.") == 0);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);
    CC_CHECK(CcGateVoiceBegin(&sim, town, &again));
    CC_CHECK(again.change.kind == next->kind);

    /* Every line of the visit has evidence, and none repeats a change. */
    for (int32_t turn = 0; turn < CC_RETURN_MAX_CHANGES; ++turn) {
        if (CcGateVoiceToldCommand(&voice, &told))
            Check(CcSimApply(&sim, &told, error, sizeof(error)));
        /* One story can explain several changes: ask again after telling. */
        if (!CcGateVoiceHasMore(&sim, &voice)) break;
        CcReturnChangeKind was = voice.change.kind;
        int32_t detail = voice.change.detail;
        CC_CHECK(CcGateVoiceNext(&sim, &voice));
        CC_CHECK(voice.change.kind != was || voice.change.detail != detail);
        CheckEvidence(&sim, &voice);
    }
    CC_CHECK(!CcGateVoiceNext(&sim, &voice));

    /* The told state is saved: the digest ranks the same after a load. */
    const char *path = "gate-voice.ccsave";
    Check(CcSaveWrite(path, &sim, error, sizeof(error)));
    Check(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CcReturnDigest loaded;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &after));
    CC_CHECK(CcReturnDigestBuild(&restored, town, &loaded));
    CC_CHECK(loaded.change_count == after.change_count);
    for (int32_t i = 0; i < after.change_count; ++i) {
        CC_CHECK(loaded.changes[i].kind == after.changes[i].kind);
        CC_CHECK(loaded.changes[i].knowledge == after.changes[i].knowledge);
        CC_CHECK(loaded.changes[i].score == after.changes[i].score);
    }
    (void)remove(path);
}

/* A world with no stories, where the company has left town 0 and the town
   has since burned. */
static CcId BurntTown(CcSim *world)
{
    CcSimInit(world, 7U);
    world->event_count = 0;
    world->event_write_index = 0;
    memset(world->gossip, 0, sizeof(world->gossip));
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        world->gossip_carriers[i].stories = 0U;
        world->gossip_carriers[i].told_player = 0U;
    }
    CcId town = world->settlements[0].id;
    CcReturnRecordSeen(world, town);
    CC_CHECK(CcReturnLastSeen(world, town) != NULL);
    world->settlements[0].fire_damage = 70;
    return town;
}

static CcCharacter *Local(CcSim *world, CcId town, int32_t skip)
{
    for (int32_t i = 0; i < world->character_count; ++i) {
        CcCharacter *person = &world->characters[i];
        if (person->current_settlement_id != town ||
            person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            CcCharacterAgeYears(world, person) < 16) continue;
        if (skip-- == 0) return person;
    }
    CC_CHECK(false);
    return NULL;
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

/* The fire story, held by one resident with the given confidence and source. */
static CcId TellFire(CcSim *world, CcId town, CcId holder, int32_t confidence,
                     CcId source)
{
    CcId event = CcMakeId(CC_ENTITY_EVENT, 900001U);
    world->gossip[4] = (CcGossip){
        .event_id = event, .origin_id = town, .day = world->current_day,
        .kind = CC_EVENT_DRAGON_RETALIATION, .settlement_mask = 1U
    };
    (void)snprintf(world->gossip[4].text, sizeof(world->gossip[4].text),
                   "Varkesh burns %s because 40 stolen crowns remain missing.",
                   world->settlements[0].name);
    CcGossipCarrier *carrier = CarrierFor(world, holder);
    carrier->stories |= UINT32_C(1) << 4U;
    carrier->versions[4] = (CcGossipVersion){
        .source_character_id = source, .confidence = confidence};
    return event;
}

static void CheckWitnessedFallback(void)
{
    CcId town = BurntTown(&sim);
    CcGateVoice voice = {0};
    CC_CHECK(CcGateVoiceBegin(&sim, town, &voice));
    CC_CHECK(voice.change.kind == CC_RETURN_CHANGE_FIRE);
    CC_CHECK(voice.change.evidence_event_id == 0U);
    /* Nobody holds a story, so the resident says what anyone can see. */
    CC_CHECK(voice.telling == CC_GATE_VOICE_SEEN);
    CC_CHECK(voice.confidence == 100);
    CC_CHECK(voice.story_slot < 0);
    CC_CHECK(voice.clause_count == 1);
    CC_CHECK(voice.clauses[0].evidence == CC_GATE_EVIDENCE_TOWN);
    CC_CHECK(strcmp(voice.line, "Fire took most of the town.") == 0);
    CheckEvidence(&sim, &voice);
    CcCommand told;
    CC_CHECK(!CcGateVoiceToldCommand(&voice, &told));
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);

    /* A resident who holds the story but not the one at the gate: the
       speaker still only says what they can see. */
    CcCharacter *holder = Local(&sim, town, 0);
    CcCharacter *speaker = Local(&sim, town, 1);
    (void)TellFire(&sim, town, holder->id, 90, 0U);
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CC_CHECK(digest.changes[0].evidence_event_id != 0U);
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &voice));
    CC_CHECK(voice.telling == CC_GATE_VOICE_SEEN);
    CC_CHECK(strstr(voice.line, "Varkesh") == NULL);
    /* The picker prefers the one who holds it. */
    CC_CHECK(CcGateVoicePickSpeaker(&sim, town, digest.changes[0].evidence_event_id) ==
             holder->id);
}

static void CheckHedgesAndAttribution(void)
{
    CcId town = BurntTown(&sim);
    CcCharacter *speaker = Local(&sim, town, 0);
    CcCharacter *teller = Local(&sim, town, 1);
    teller->occupation = CC_OCCUPATION_INNKEEPER;
    char first[CC_NAME_CAPACITY];
    (void)snprintf(first, sizeof(first), "%s", teller->name);
    first[strcspn(first, " ")] = '\0';
    char expected[160];
    /* Middling confidence: the burning, the hedged reason, and who told them:
       "Varkesh burned most of the town. Folk say it was over missing hoard
       money — I heard it from Thora at the inn." */
    (void)snprintf(expected, sizeof(expected),
                   "Varkesh burned most of the town. Folk say it was over missing "
                   "hoard money — I heard it from %s at the inn.", first);
    (void)TellFire(&sim, town, speaker->id, 55, teller->id);
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CcGateVoice voice = {0};
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &voice));
    CC_CHECK(voice.telling == CC_GATE_VOICE_HEARD);
    CC_CHECK(strcmp(voice.line, expected) == 0);
    CC_CHECK(voice.clause_count == 3);
    CC_CHECK(voice.clauses[0].part == CC_GATE_PART_EVENT);
    CC_CHECK(voice.clauses[1].part == CC_GATE_PART_CAUSE);
    CC_CHECK(voice.clauses[2].part == CC_GATE_PART_SOURCE);
    CC_CHECK(voice.clauses[2].evidence_id == teller->id);
    CC_CHECK(strcmp(voice.source_name, teller->name) == 0);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);

    /* Doubtful: the asked role (who did it) is withheld, and hedged. */
    (void)TellFire(&sim, town, speaker->id, 25, teller->id);
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &voice));
    CC_CHECK(voice.chosen_fact >= 0);
    CC_CHECK(voice.facts[voice.chosen_fact].certainty == CC_GATE_CERTAIN_DOUBTFUL);
    CC_CHECK(strstr(voice.line, "Varkesh") == NULL);
    CC_CHECK(strncmp(voice.line, "Fire took most of the town. Some say it was over", 48) == 0);
    CC_CHECK(voice.clauses[0].evidence == CC_GATE_EVIDENCE_TOWN);
    CC_CHECK(strstr(voice.line, "but I'm not sure of it.") != NULL);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);

    /* Their own eyes: no hedge, and they say so. */
    (void)TellFire(&sim, town, speaker->id, 100, speaker->id);
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &voice));
    CC_CHECK(voice.facts[voice.chosen_fact].certainty == CC_GATE_CERTAIN_WITNESSED);
    CC_CHECK(strcmp(voice.line, "Varkesh burned most of the town. It was over "
                                "missing hoard money. I saw it myself.") == 0);
    CheckEvidence(&sim, &voice);

    /* The line builder is deterministic. */
    CcGateVoice again = {0};
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &again));
    CC_CHECK(strcmp(again.line, voice.line) == 0);
    CcSpeech speech;
    CC_CHECK(CcGateVoiceSpeech(&sim, &voice, &speech));
    CC_CHECK(strcmp(speech.text, voice.line) == 0);
    CC_CHECK(speech.speaker_id == speaker->id);
}

/* A story that explains a visible change comes after it: the bare stall,
   then the raid, then the hedge. The town is "the town", not its name. */
static void CheckCauseFollowsEvent(void)
{
    CcId town = BurntTown(&sim);
    CcSettlement *place = &sim.settlements[0];
    place->fire_damage = 0;
    sim.return_memory.towns[0].stock[CC_GOOD_BREAD] = 9;
    place->stock[CC_GOOD_BREAD] = 0;
    CcCharacter *speaker = Local(&sim, town, 0);
    CcId event = CcMakeId(CC_ENTITY_EVENT, 900002U);
    sim.gossip[6] = (CcGossip){
        .event_id = event, .origin_id = town, .day = sim.current_day,
        .kind = CC_EVENT_SETTLEMENT_RAIDED, .settlement_mask = 1U
    };
    (void)snprintf(sim.gossip[6].text, sizeof(sim.gossip[6].text),
                   "The Cinder Tithe raids %s and takes 9 Bread.", place->name);
    CcGossipCarrier *carrier = CarrierFor(&sim, speaker->id);
    carrier->stories |= UINT32_C(1) << 6U;
    carrier->versions[6] = (CcGossipVersion){.confidence = 80};
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, town, &digest));
    CC_CHECK(digest.changes[0].kind == CC_RETURN_CHANGE_STALL_EMPTY);
    CC_CHECK(digest.changes[0].evidence_event_id == event);
    CcGateVoice voice = {0};
    CC_CHECK(CcGateVoiceSay(&sim, speaker->id, &digest.changes[0], &voice));
    CC_CHECK(voice.telling == CC_GATE_VOICE_HEARD);
    CC_CHECK(voice.clause_count == 2);
    CC_CHECK(voice.clauses[0].part == CC_GATE_PART_EVENT);
    CC_CHECK(voice.clauses[0].evidence == CC_GATE_EVIDENCE_TOWN);
    CC_CHECK(voice.clauses[1].part == CC_GATE_PART_CAUSE);
    CC_CHECK(voice.clauses[1].evidence == CC_GATE_EVIDENCE_STORY);
    CC_CHECK(strcmp(voice.line, "There's no bread in the market. "
                                "The Cinder Tithe raided the town, I hear.") == 0);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);
}

static void CheckFaces(void)
{
    CcId town = BurntTown(&sim);
    CcCharacter *face = Local(&sim, town, 2);
    CcTownSeen *seen = &sim.return_memory.towns[0];
    /* A remembered face who has died is not at the gate. */
    seen->face_ids[0] = CcMakeId(CC_ENTITY_CHARACTER, 0xfffff0U);
    (void)snprintf(seen->face_names[0], sizeof(seen->face_names[0]), "Old Wenna");
    seen->face_ids[1] = face->id;
    (void)snprintf(seen->face_names[1], sizeof(seen->face_names[1]), "%s", face->name);
    CC_CHECK(CcGateVoicePickSpeaker(&sim, town, 0U) == face->id);
    CcGateVoice voice = {0};
    CC_CHECK(CcGateVoiceBegin(&sim, town, &voice));
    CC_CHECK(voice.speaker_id == face->id);
    CC_CHECK(voice.remembered_face);
    CC_CHECK(voice.clauses[0].evidence == CC_GATE_EVIDENCE_FACE);
    CC_CHECK(strncmp(voice.line, "You're back.", 12) == 0);
    CheckEvidence(&sim, &voice);
    (void)printf("%s: \"%s\"\n", voice.speaker, voice.line);

    /* With every face gone, a local speaks instead. */
    face->current_settlement_id = sim.settlements[1].id;
    CcId local = CcGateVoicePickSpeaker(&sim, town, 0U);
    CC_CHECK(local != 0U && local != face->id);
    CC_CHECK(CcGateVoiceBegin(&sim, town, &voice));
    CC_CHECK(voice.speaker_id == local);
    CC_CHECK(!voice.remembered_face);
    CheckEvidence(&sim, &voice);

    /* First visits have no voice. */
    CC_CHECK(!CcGateVoiceBegin(&sim, sim.settlements[2].id, &voice));
}

int main(void)
{
    CheckWitnessedFallback();
    CheckHedgesAndAttribution();
    CheckCauseFollowsEvent();
    CheckFaces();
    CheckRouteVoice();
    (void)printf("gate voice tests passed\n");
    return 0;
}
