#include "persistence/cc_save.h"
#include "sim/cc_return.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <string.h>

/* The Return: the company's last view of a town and the change digest. */

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

static const CcReturnChange *FindKind(const CcReturnDigest *digest,
                                      CcReturnChangeKind kind)
{
    for (int32_t i = 0; i < digest->change_count; ++i)
        if (digest->changes[i].kind == kind) return &digest->changes[i];
    return NULL;
}

static int32_t RankOf(const CcReturnDigest *digest, CcReturnChangeKind kind)
{
    for (int32_t i = 0; i < digest->change_count; ++i)
        if (digest->changes[i].kind == kind) return i;
    return -1;
}

/* A world with no recent events or stories, so each check supplies its own. */
static void QuietWorld(CcSim *world)
{
    CcSimInit(world, 7U);
    world->event_count = 0;
    world->event_write_index = 0;
    memset(world->gossip, 0, sizeof(world->gossip));
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        world->gossip_carriers[i].stories = 0U;
        world->gossip_carriers[i].told_player = 0U;
    }
}

static void CheckCaptureAndRecord(void)
{
    CcSimInit(&sim, 11U);
    CcId thornford = TownByName(&sim, "Thornford");
    CC_CHECK(sim.player.location_id == thornford);
    CC_CHECK(CcReturnLastSeen(&sim, thornford) == NULL);
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&sim, thornford, &digest));
    CC_CHECK(digest.first_visit && digest.change_count == 0);

    /* The company has met one resident; that face is remembered. */
    CcCharacter *face = NULL;
    for (int32_t i = 0; i < sim.character_count && face == NULL; ++i)
        if (sim.characters[i].home_settlement_id == thornford &&
            sim.characters[i].id != sim.kingdoms[0].ruler_character_id)
            face = &sim.characters[i];
    CC_CHECK(face != NULL);
    face->introduced_day = sim.current_day;
    Ride(&sim, TownByName(&sim, "Gloamgate"));
    const CcTownSeen *seen = CcReturnLastSeen(&sim, thornford);
    CC_CHECK(seen != NULL);
    CC_CHECK(seen->kingdom_id == sim.settlements[0].kingdom_id);
    CC_CHECK(seen->ruler_id != 0U && seen->ruler_name[0] != '\0');
    CC_CHECK(seen->face_ids[0] == face->id);
    CC_CHECK(strcmp(seen->face_names[0], face->name) == 0);
    CC_CHECK(seen->population > 0);
    Check(CcSimValidate(&sim, error, sizeof(error)));
    /* Gloamgate is not remembered until the company leaves it. */
    CC_CHECK(CcReturnLastSeen(&sim, TownByName(&sim, "Gloamgate")) == NULL);

    /* The record is saved state: it changes the hash and must validate. */
    uint64_t hash = CcSimHash(&sim);
    sim.return_memory.towns[0].hunger += 1;
    CC_CHECK(CcSimHash(&sim) != hash);
    sim.return_memory.towns[0].hunger -= 1;
    CC_CHECK(CcSimHash(&sim) == hash);
    sim.return_memory.towns[0].seen_day = sim.current_day + 1;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim.return_memory.towns[0].seen_day = seen->seen_day;
}

static void CheckFireOutranksPrice(void)
{
    QuietWorld(&sim);
    CcId town = sim.settlements[0].id;
    CcTownSeen before, now;
    CC_CHECK(CcReturnCapture(&sim, town, &before));
    now = before;
    now.seen_day = before.seen_day + 20;
    now.fire_damage = 60;
    now.conditions |= CC_TOWN_BURNT;
    int32_t good = CC_GOOD_WOOL;
    CC_CHECK(before.price[good] >= 4);
    now.price[good] = before.price[good] * 2;
    CcReturnDigest digest;
    CcReturnCompare(&sim, &before, &now, &digest);
    CC_CHECK(digest.change_count == 2);
    CC_CHECK(digest.changes[0].kind == CC_RETURN_CHANGE_FIRE);
    CC_CHECK(digest.changes[0].before == 0 && digest.changes[0].after == 60);
    CC_CHECK(digest.changes[1].kind == CC_RETURN_CHANGE_PRICE);
    CC_CHECK(digest.changes[1].detail == good);
    CC_CHECK(digest.changes[0].score > digest.changes[1].score);

    /* A small price move is not news at all. */
    now = before;
    now.seen_day = before.seen_day + 20;
    now.price[good] = before.price[good] + 1;
    CcReturnCompare(&sim, &before, &now, &digest);
    CC_CHECK(digest.found_count == 0);

    /* A dead face and a new ruler outrank a hungrier town. */
    now = before;
    now.seen_day = before.seen_day + 20;
    now.hunger = before.hunger + 20;
    now.ruler_id = sim.characters[1].id;
    (void)snprintf(now.ruler_name, sizeof(now.ruler_name), "%s", sim.characters[1].name);
    CcReturnCompare(&sim, &before, &now, &digest);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_NEW_RULER) == 0);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_HUNGER) == 1);
    char text[256];
    CcReturnDescribe(&sim, &digest.changes[0], text, sizeof(text));
    CC_CHECK(strstr(text, sim.characters[1].name) != NULL);
    CC_CHECK(strstr(text, before.ruler_name) != NULL);
}

static void CheckHeardNewsRanksLower(void)
{
    QuietWorld(&sim);
    CcId town = sim.settlements[0].id;
    CcTownSeen before, now;
    CC_CHECK(CcReturnCapture(&sim, town, &before));
    now = before;
    now.seen_day = before.seen_day + 20;
    now.fire_damage = 60;
    now.hunger = 50;
    now.conditions |= CC_TOWN_BURNT | CC_TOWN_HUNGRY;
    before.hunger = 10;
    before.conditions &= ~(uint32_t)CC_TOWN_HUNGRY;
    CcReturnDigest digest;
    CcReturnCompare(&sim, &before, &now, &digest);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_FIRE) == 0);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_HUNGER) == 1);
    CC_CHECK(digest.changes[0].knowledge == CC_RETURN_UNKNOWN);
    int32_t unknown_score = digest.changes[0].score;

    /* A story about the fire reached the gossip ledger but not the company. */
    CcId fire_event = CcMakeId(CC_ENTITY_EVENT, 900001U);
    sim.gossip[3] = (CcGossip){
        .event_id = fire_event, .origin_id = town,
        .day = before.seen_day + 5, .kind = CC_EVENT_DRAGON_RETALIATION
    };
    (void)snprintf(sim.gossip[3].text, sizeof(sim.gossip[3].text),
                   "The dragon burns %s.", sim.settlements[0].name);
    CcId teller = sim.characters[2].id;
    CcGossipCarrier *carrier = &sim.gossip_carriers[5];
    carrier->id = teller;
    carrier->stories = UINT32_C(1) << 3U;
    carrier->versions[3].confidence = 70;
    CcReturnCompare(&sim, &before, &now, &digest);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_FIRE) == 0);
    CC_CHECK(digest.changes[0].evidence_event_id == fire_event);
    CC_CHECK(digest.changes[0].knowledge == CC_RETURN_UNKNOWN);

    /* Once that person has told the company, the fire ranks below hunger. */
    carrier->told_player = UINT32_C(1) << 3U;
    CcReturnCompare(&sim, &before, &now, &digest);
    const CcReturnChange *fire = FindKind(&digest, CC_RETURN_CHANGE_FIRE);
    CC_CHECK(fire != NULL);
    CC_CHECK(fire->knowledge == CC_RETURN_TOLD);
    CC_CHECK(fire->source_id == teller);
    CC_CHECK(fire->source_confidence == 70);
    CC_CHECK(fire->score < unknown_score);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_HUNGER) == 0);
    CC_CHECK(RankOf(&digest, CC_RETURN_CHANGE_FIRE) == 1);
    char text[2048];
    CC_CHECK(CcReturnDigestText(&sim, &digest, text, sizeof(text)));
    CC_CHECK(strstr(text, "told by") != NULL);
}

static void CheckCodecAndSave(void)
{
    const char *path = "return-memory.ccsave";
    CcSimInit(&sim, 21U);
    Ride(&sim, TownByName(&sim, "Gloamgate"));
    CC_CHECK(CcReturnLastSeen(&sim, TownByName(&sim, "Thornford")) != NULL);

    uint8_t bytes[4096], again[4096];
    size_t length = CcReturnMemoryEncode(&sim.return_memory, bytes, sizeof(bytes));
    CC_CHECK(length == CcReturnMemoryEncodedSize(&sim.return_memory));
    CcReturnMemory decoded;
    CC_CHECK(CcReturnMemoryDecode(&decoded, bytes, length));
    CC_CHECK(CcReturnMemoryHash(&decoded) == CcReturnMemoryHash(&sim.return_memory));
    CC_CHECK(!CcReturnMemoryDecode(&decoded, bytes, length - 1U));
    bytes[0] ^= 0xffU;
    CC_CHECK(!CcReturnMemoryDecode(&decoded, bytes, length));
    bytes[0] ^= 0xffU;

    Check(CcSaveWrite(path, &sim, error, sizeof(error)));
    Check(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(CcReturnMemoryEncode(&restored.return_memory, again, sizeof(again)) == length);
    CC_CHECK(memcmp(bytes, again, length) == 0);
    (void)remove(path);

    /* The previous schema had no memory: it loads with none, and the next
       departure starts one. */
    CcSimInit(&sim, 21U);
    CcTestStampLegacyGoods(&sim, CC_RETURN_SCHEMA_VERSION - 1U);
    Ride(&sim, TownByName(&sim, "Gloamgate"));
    CC_CHECK(CcReturnLastSeen(&sim, TownByName(&sim, "Thornford")) == NULL);
    uint64_t legacy_hash = CcSimHash(&sim);
    Check(CcSaveWrite(path, &sim, error, sizeof(error)));
    Check(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcReturnLastSeen(&restored, TownByName(&restored, "Thornford")) == NULL);
    Check(CcSimValidate(&restored, error, sizeof(error)));
    restored.schema_version = CC_RETURN_SCHEMA_VERSION - 1U;
    CC_CHECK(CcSimHash(&restored) == legacy_hash);
    restored.schema_version = CC_SIM_SCHEMA_VERSION;
    CcReturnDigest digest;
    CC_CHECK(CcReturnDigestBuild(&restored, TownByName(&restored, "Thornford"), &digest));
    CC_CHECK(digest.first_visit);
    Ride(&restored, TownByName(&restored, "Thornford"));
    CC_CHECK(CcReturnLastSeen(&restored, TownByName(&restored, "Gloamgate")) != NULL);
    (void)remove(path);
}

/* Thornford to Silverwick by the real roads, a long wait, and back. */
static void RunReturn(CcSim *world, uint32_t seed, int32_t days,
                      CcReturnDigest *gloamgate, CcReturnDigest *thornford)
{
    CcSimInit(world, seed);
    CcId home = TownByName(world, "Thornford");
    CcId middle = TownByName(world, "Gloamgate");
    CcId away = TownByName(world, "Silverwick");
    Ride(world, middle);
    Ride(world, away);
    CcSimAdvanceDays(world, days);
    Ride(world, middle);
    CC_CHECK(CcReturnDigestBuild(world, middle, gloamgate));
    Ride(world, home);
    CC_CHECK(CcReturnDigestBuild(world, home, thornford));
}

static bool Meaningful(const CcReturnDigest *digest)
{
    for (int32_t i = 0; i < digest->change_count; ++i) {
        CcReturnChangeKind kind = digest->changes[i].kind;
        if (kind != CC_RETURN_CHANGE_PRICE &&
            kind != CC_RETURN_CHANGE_STALL_RESTOCKED &&
            digest->changes[i].score >= 150) return true;
    }
    return false;
}

static void CheckRouteScenario(void)
{
    CcReturnDigest gloamgate, thornford, gloamgate_again, thornford_again;
    RunReturn(&sim, 4U, 365, &gloamgate, &thornford);
    CC_CHECK(!gloamgate.first_visit && !thornford.first_visit);
    CC_CHECK(thornford.return_day - thornford.seen_day >= 365);
    CC_CHECK(gloamgate.change_count > 0 && thornford.change_count > 0);
    CC_CHECK(Meaningful(&gloamgate) || Meaningful(&thornford));
    for (int32_t i = 1; i < gloamgate.change_count; ++i)
        CC_CHECK(gloamgate.changes[i - 1].score >= gloamgate.changes[i].score);
    char text[8192], text_again[8192];
    CC_CHECK(CcReturnDigestText(&sim, &gloamgate, text, sizeof(text)));
    (void)printf("%s", text);
    CC_CHECK(CcReturnDigestText(&sim, &thornford, text, sizeof(text)));
    (void)printf("%s", text);

    /* The same seed gives the same world, the same memory, and the same digest. */
    RunReturn(&other, 4U, 365, &gloamgate_again, &thornford_again);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&other));
    CC_CHECK(CcReturnDigestText(&other, &thornford_again, text_again, sizeof(text_again)));
    CC_CHECK(strcmp(text, text_again) == 0);
    CC_CHECK(gloamgate.change_count == gloamgate_again.change_count);
    for (int32_t i = 0; i < gloamgate.change_count; ++i) {
        CC_CHECK(gloamgate.changes[i].kind == gloamgate_again.changes[i].kind);
        CC_CHECK(gloamgate.changes[i].score == gloamgate_again.changes[i].score);
        CC_CHECK(gloamgate.changes[i].evidence_event_id ==
                 gloamgate_again.changes[i].evidence_event_id);
    }
    /* Building the digest reads the world without changing it. */
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcReturnDigestBuild(&sim, TownByName(&sim, "Gloamgate"), &gloamgate_again));
    CC_CHECK(CcSimHash(&sim) == hash);
}

int main(void)
{
    CheckCaptureAndRecord();
    CheckFireOutranksPrice();
    CheckHeardNewsRanksLower();
    CheckCodecAndSave();
    CheckRouteScenario();
    (void)printf("return tests passed\n");
    return 0;
}
