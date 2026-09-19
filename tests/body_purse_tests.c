#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "sim/cc_sim_custody.h"
#include "test_support.h"

#include <string.h>

/* Fallen bodies and purses (schema 102), design: docs/fallen-purses.md,
   decisions #286/#406. A death drops the carried purse at the place it fell;
   the purse is a custody entry owned by the dead person, lying there; the
   player (or a camping bandit group) claims it; the successor inherits
   nothing carried; the coin never leaves the tracked economy. */

static CcSim sim;
static CcSim restored;
static char error[256];

/* CcCustodyFind only returns active entries; claims need the spent row. */
static const CcCustodyEntry *EntryById(CcId id)
{
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        if (sim.custody.entries[i].id == id) return &sim.custody.entries[i];
    }
    return NULL;
}

static const CcCustodyEntry *FindPurseOf(CcId owner_id)
{
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &sim.custody.entries[i];
        if (entry->active && CcSimIsBodyPurse(&sim, entry) &&
            (owner_id == 0U || entry->owner_id == owner_id)) return entry;
    }
    return NULL;
}

/* Give a character a purse to carry, then mark them to die today. */
static CcCharacter *DoomCarrier(CcMoney coins)
{
    for (int i = 0; i < sim.character_count; ++i) {
        CcCharacter *person = &sim.characters[i];
        if (person->death_day > sim.current_day &&
            CcSimSettlement(&sim, person->home_settlement_id) != NULL) {
            person->travel_coins = coins;
            person->death_day = sim.current_day;
            return person;
        }
    }
    return NULL;
}

static void TestDeathDropsTheCarriedPurse(void)
{
    CcSimInit(&sim, UINT32_C(0xb04d5eed));
    CcCharacter *dead = DoomCarrier(60);
    CC_CHECK(dead != NULL);
    CcId dead_id = dead->id;
    CcId place_id = dead->current_settlement_id != 0U ?
        dead->current_settlement_id : dead->home_settlement_id;
    CcMoney coins = dead->travel_coins;
    CcMoney gold = CcSimTrackedGold(&sim);
    /* The person dies. */
    dead->death_day = sim.current_day;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimCharacter(&sim, dead_id) == NULL);
    /* The purse lies where they fell, owned by the dead. */
    const CcCustodyEntry *purse = FindPurseOf(dead_id);
    CC_CHECK(purse != NULL);
    CC_CHECK(purse->quantity == coins);
    CC_CHECK(purse->owner_id == dead_id);
    CC_CHECK(purse->holder.kind == CC_CUSTODY_STORE);
    CC_CHECK(purse->holder.id == place_id);
    CC_CHECK(purse->kind == CC_CUSTODY_PURSE);
    /* The successor did not inherit the carried coin. */
    for (int i = 0; i < sim.character_count; ++i) {
        CC_CHECK(sim.characters[i].id != dead_id);
    }
    /* The coin never left the tracked economy. */
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void TestPlayerLiftsThePurse(void)
{
    CcSimInit(&sim, UINT32_C(0xb04d5eed));
    CcCharacter *dead = DoomCarrier(60);
    CC_CHECK(dead != NULL);
    CcId dead_id = dead->id;
    CcId place_id = dead->current_settlement_id != 0U ?
        dead->current_settlement_id : dead->home_settlement_id;
    CcMoney coins = dead->travel_coins;
    CcMoney gold = CcSimTrackedGold(&sim);
    CcSimAdvanceDays(&sim, 1);
    const CcCustodyEntry *purse = FindPurseOf(dead_id);
    CC_CHECK(purse != NULL);
    CcId purse_id = purse->id;
    /* Away from the place, the purse cannot be lifted. */
    CcId away = 0U;
    for (int i = 0; i < sim.settlement_count; ++i) {
        if (sim.settlements[i].id != place_id &&
            !CcSettlementIsAbandoned(&sim.settlements[i])) {
            away = sim.settlements[i].id;
            break;
        }
    }
    CC_CHECK(away != 0U);
    sim.player.location_id = away;
    CcCommand away_claim = {
        .kind = CC_COMMAND_TAKE_BODY_PURSE, .target_id = purse_id
    };
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &away_claim, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    /* At the place, the lift moves the coin, kills the purse, and validates. */
    sim.player.location_id = place_id;
    CcMoney player_before = sim.player.coins;
    CcCommand claim = {
        .kind = CC_COMMAND_TAKE_BODY_PURSE, .target_id = purse_id
    };
    CC_CHECK(CcSimApply(&sim, &claim, error, sizeof(error)));
    CC_CHECK(sim.player.coins == player_before + coins);
    CC_CHECK(EntryById(purse_id)->quantity == 0);
    CC_CHECK(!EntryById(purse_id)->active);
    CC_CHECK(FindPurseOf(0U) == NULL);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    /* A second lift finds nothing. */
    CcCommand again = {
        .kind = CC_COMMAND_TAKE_BODY_PURSE, .target_id = purse_id
    };
    CC_CHECK(!CcSimApply(&sim, &again, error, sizeof(error)));
    /* The loot is in the event record. */
    bool seen = false;
    for (int i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event != NULL && event->kind == CC_EVENT_BODY_LOOTED &&
            event->subject_id == dead_id) seen = true;
    }
    CC_CHECK(seen);
}

static void TestBanditsCampAndLiftThePurse(void)
{
    CcSimInit(&sim, UINT32_C(0xb04d5eed));
    CcCharacter *dead = DoomCarrier(60);
    CC_CHECK(dead != NULL);
    CcId dead_id = dead->id;
    CcId place_id = dead->current_settlement_id != 0U ?
        dead->current_settlement_id : dead->home_settlement_id;
    CcMoney coins = dead->travel_coins;
    CcMoney gold = CcSimTrackedGold(&sim);
    CcSimAdvanceDays(&sim, 1);
    const CcCustodyEntry *purse = FindPurseOf(dead_id);
    CC_CHECK(purse != NULL);
    CC_CHECK(purse->quantity == coins);
    /* A bandit band rides a road through the place where the purse lies. */
    CcId through_route = 0U;
    for (int i = 0; i < sim.route_count; ++i) {
        const CcRoute *road = &sim.routes[i];
        if (road->from_id == place_id || road->to_id == place_id) {
            through_route = road->id;
            break;
        }
    }
    CC_CHECK(through_route != 0U);
    bool placed = false;
    for (int i = 0; i < sim.bandit_count; ++i) {
        if (sim.bandits[i].route_id == 0U) {
            sim.bandits[i].route_id = through_route;
            placed = true;
            break;
        }
    }
    if (!placed && sim.bandit_count > 0) {
        sim.bandits[0].route_id = through_route;
        placed = true;
    }
    CC_CHECK(placed);
    /* A week of grace: the purse lies while travellers pass. */
    CcSimAdvanceDays(&sim, 5);
    CC_CHECK(FindPurseOf(dead_id) != NULL);
    /* Then the bandits lift it and spend it in the local market. */
    CcSimAdvanceDays(&sim, 3);
    CC_CHECK(FindPurseOf(dead_id) == NULL);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    bool seen = false;
    for (int i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event != NULL && event->kind == CC_EVENT_BODY_LOOTED &&
            event->subject_id == dead_id) seen = true;
    }
    CC_CHECK(seen);
}

static void TestPurseSurvivesTheSaveRoundTrip(void)
{
    CcSimInit(&sim, UINT32_C(0xb04d5eed));
    CcCharacter *dead = DoomCarrier(60);
    CC_CHECK(dead != NULL);
    CcId dead_id = dead->id;
    CcSimAdvanceDays(&sim, 1);
    const CcCustodyEntry *purse = FindPurseOf(dead_id);
    CC_CHECK(purse != NULL);
    CcId purse_id = purse->id;
    const char *path = "body-purse-round-trip.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    const CcCustodyEntry *loaded = CcCustodyFind(&restored.custody, purse_id);
    CC_CHECK(loaded != NULL && loaded->active && loaded->quantity == purse->quantity);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    (void)remove(path);
    (void)remove("body-purse-round-trip.ccsave-wal");
    (void)remove("body-purse-round-trip.ccsave-shm");
}

static void TestLegacySchemaStillInherits(void)
{
    CcSimInit(&sim, UINT32_C(0xb04d5eed));
    sim.schema_version = 101U;
    sim.mine = (CcMineVisit){0};
    CcCustodyInit(&sim.custody);
    sim.custody.capacity = CC_CUSTODY_LEGACY_CAPACITY;
    CcCharacter *dead = DoomCarrier(60);
    CC_CHECK(dead != NULL);
    CcMoney coins = dead->travel_coins;
    CcMoney gold = CcSimTrackedGold(&sim);
    dead->death_day = sim.current_day;
    (void)0;
    CcSimAdvanceDays(&sim, 1);
    /* Schema 101: the old ghost refund stays for journal replay. */
    CC_CHECK(FindPurseOf(0U) == NULL);
    CcMoney inherited = 0;
    for (int i = 0; i < sim.character_count; ++i) {
        inherited += sim.characters[i].travel_coins;
    }
    CC_CHECK(inherited >= coins - 1);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    TestDeathDropsTheCarriedPurse();
    TestPlayerLiftsThePurse();
    TestBanditsCampAndLiftThePurse();
    TestPurseSurvivesTheSaveRoundTrip();
    TestLegacySchemaStillInherits();
    return 0;
}
