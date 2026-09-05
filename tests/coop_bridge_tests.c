#include "multiplayer/cc_coop.h"
#include "multiplayer/cc_coop_commands.h"
#include "test_support.h"
#include "persistence/cc_save.h"
#include <stdlib.h>
#include <string.h>

static void CheckCommandRoundTrips(void)
{
    CcSim *direct = CcCoopCreate(42U);
    CcSim *shared = CcCoopCreate(42U);
    CC_CHECK(direct != NULL && shared != NULL);
    for (int32_t kind = 1; kind <= (int32_t)CC_COMMAND_PARTY_WIPE; ++kind) {
        CcSimInit(direct, 42U);
        *shared = *direct;
        const char *name = CcCoopActionName((CcCommandKind)kind);
        CC_CHECK(name[0] != '\0');
        for (int32_t prior = 1; prior < kind; ++prior)
            CC_CHECK(strcmp(name, CcCoopActionName((CcCommandKind)prior)) != 0);
        CcCommand command = {.kind = (CcCommandKind)kind, .good = CC_GOOD_BREAD,
                              .amount = 1, .dungeon_state = (CcDungeonState)1};
        char direct_error[256] = "", shared_error[256] = "";
        bool expected = CcSimApply(direct, &command, direct_error, sizeof(direct_error));
        bool actual = CcCoopApply(shared, name, 0U, CC_GOOD_BREAD, 1,
                                  shared_error, sizeof(shared_error));
        CC_CHECK(actual == expected);
        CC_CHECK(strcmp(direct_error, shared_error) == 0);
        if (expected) CC_CHECK(CcSimHash(direct) == CcSimHash(shared));
    }
    CcCoopDestroy(direct);
    CcCoopDestroy(shared);
}

static void CheckPartyWipe(void)
{
    char error[256];
    CcSim *sim = CcCoopCreate(42U);
    CcSim *replayed = CcCoopCreate(42U);
    CC_CHECK(sim != NULL && replayed != NULL);
    int32_t day = sim->current_day;
    CcId ancestor = sim->characters[0].id;
    sim->characters[0].death_day = day + 1;
    CcCommand wipe = {.kind = CC_COMMAND_PARTY_WIPE, .target_id = (CcId)day};
    (void)remove("party-wipe-test.ccsave");
    CcJournal *journal = CcJournalStart("party-wipe-test.ccsave", sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, sim, &wipe, error, sizeof(error)));
    CC_CHECK(sim->current_day == day + CC_PARTY_WIPE_DAYS);
    CC_CHECK(sim->characters[0].id != ancestor && sim->character_deaths > 0);
    CC_CHECK(sim->world_seed == 42U);
    CC_CHECK(CcSimValidate(sim, error, sizeof(error)));
    uint64_t hash = CcSimHash(sim);
    CC_CHECK(!CcSimApply(sim, &wipe, error, sizeof(error)));
    CC_CHECK(CcSimHash(sim) == hash);
    CcJournalAbandon(&journal);
    journal = CcJournalResume("party-wipe-test.ccsave", replayed, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(replayed) == hash);
    CC_CHECK(CcJournalClose(&journal, replayed, error, sizeof(error)));
    (void)remove("party-wipe-test.ccsave");

    // Each active trip returns to the carriage before the world advances.
    for (int32_t trip = 0; trip < 3; ++trip) {
        CcSimInit(sim, 42U);
        if (trip == 0) {
            CC_CHECK(CcCoopApply(sim, "travel", sim->settlements[1].id,
                                0, 0, error, sizeof(error)));
        } else if (trip == 1) {
            sim->player.location_id = sim->dungeons[0].settlement_id;
            sim->carriage.location_id = sim->player.location_id;
            sim->player.cargo[CC_GOOD_BREAD] = 4;
            CC_CHECK(CcCoopApply(sim, "enter_dungeon", sim->dungeons[0].id,
                                0, 0, error, sizeof(error)));
        } else {
            sim->pony_company.encounter = 0;
        }
        day = sim->current_day;
        CC_CHECK(CcCoopApply(sim, "party_wipe", (CcId)day, 0, 0,
                            error, sizeof(error)));
        CC_CHECK(sim->current_day == day + CC_PARTY_WIPE_DAYS);
        CC_CHECK(!sim->journey.active && !sim->dungeon_expedition.active);
        CC_CHECK(sim->pony_company.encounter == -1);
        CC_CHECK(sim->carriage.mode == CC_CARRIAGE_PARKED);
        CC_CHECK(sim->carriage.location_id == sim->player.location_id);
        unsigned char *bytes = NULL;
        size_t length = 0;
        CC_CHECK(CcCoopEncode(sim, &bytes, &length, error, sizeof(error)));
        CC_CHECK(CcCoopDecode(replayed, bytes, length, error, sizeof(error)));
        CcCoopFree(bytes);
        CC_CHECK(CcSimHash(sim) == CcSimHash(replayed));
    }
    sim->current_day = CC_SIM_MAX_DAY - CC_PARTY_WIPE_DAYS + 1;
    wipe.target_id = (CcId)sim->current_day;
    hash = CcSimHash(sim);
    CC_CHECK(!CcSimApply(sim, &wipe, error, sizeof(error)));
    CC_CHECK(CcSimHash(sim) == hash);
    CcCoopDestroy(sim);
    CcCoopDestroy(replayed);
}

static void CheckSharedPonies(void)
{
    char error[256];
    CcSim *host = CcCoopCreate(117U);
    CcSim *guest = CcCoopCreate(42U);
    CC_CHECK(host != NULL && guest != NULL);
    CC_CHECK(CcCoopApply(host, "travel", host->settlements[1].id, 0, 0, error, sizeof(error)));
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        if (host->pony_company.ponies[i].route_id != 0U)
            host->pony_company.ponies[i].route_id = host->journey.route_id;
    }
    for (int32_t tick = 0; tick < 10000 && CcPonyOnRoad(host) < 0; ++tick)
        CC_CHECK(CcCoopAdvance(host, 1, error, sizeof(error)));
    int32_t pony = CcPonyOnRoad(host);
    CC_CHECK(pony >= 0);
    CcId target = (CcId)pony + 1U;
    CcGood good = CcPonyQuestGood(&host->pony_company.ponies[pony]);
    host->player.cargo[good] = host->pony_company.ponies[pony].quest_amount;
    CC_CHECK(CcCoopApply(host, "meet_pony", target, 0, 0, error, sizeof(error)));
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    CC_CHECK(CcCoopApply(guest, "leave_pony", target, 0, 0, error, sizeof(error)));
    CC_CHECK(guest->pony_company.encounter == -1);
    CC_CHECK(CcCoopApply(host, "help_pony", target, 0, 0, error, sizeof(error)));
    int32_t released = host->pony_company.team[1];
    CC_CHECK(CcCoopApply(host, "swap_pony", target, 0, 1, error, sizeof(error)));
    CC_CHECK(host->pony_company.team[1] == pony);
    CC_CHECK(host->pony_company.ponies[released].releases == 1);
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    CcCoopDestroy(host);
    CcCoopDestroy(guest);
}

static void CheckSharedDepartureAndRoadStop(void)
{
    char error[256];
    CcSim *host = CcCoopCreate(42U);
    CcSim *guest = CcCoopCreate(42U);
    CC_CHECK(host != NULL && guest != NULL);
    host->player.coins = 0;
    CC_CHECK(CcCoopApply(host, "travel", host->settlements[1].id, 0, 0, error, sizeof(error)));
    CC_CHECK(host->journey.active && host->player.coins == 0);
    const CcRoadSite *site = NULL;
    for (int32_t i = 0; i < host->road_site_count; ++i) {
        if (host->road_sites[i].route_id == host->journey.route_id) {
            site = &host->road_sites[i];
            break;
        }
    }
    CC_CHECK(site != NULL);
    host->journey.ambush_pending = false;
    host->journey.encounter_triggered = true;
    host->carriage.progress_milli = site->progress_milli;
    host->journey.elapsed_subticks = (int32_t)(
        ((int64_t)host->journey.total_subticks * site->progress_milli + 999) / 1000);
    CC_CHECK(CcSimJourneyRoadSiteStop(host) == site);
    uint64_t stopped = CcSimHash(host);
    CC_CHECK(CcCoopApply(host, "skip_watch", 0U, 0, 0, error, sizeof(error)));
    CC_CHECK(CcSimHash(host) == stopped);
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(guest) == stopped && CcSimJourneyRoadSiteStop(guest) != NULL);
    CC_CHECK(CcCoopApply(guest, "pass_road_site", site->id, 0, 0, error, sizeof(error)));
    int32_t before = host->carriage.progress_milli;
    CC_CHECK(CcCoopAdvance(host, 60, error, sizeof(error)));
    CC_CHECK(host->carriage.progress_milli > before);
    CC_CHECK(CcCoopAdvance(host, 3600, error, sizeof(error)));
    CC_CHECK(!host->journey.active || host->journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(CcSimJourneyRoadSiteStop(host) != site);
    CcCoopDestroy(host);
    CcCoopDestroy(guest);
}

int main(void)
{
    CheckCommandRoundTrips();
    CheckPartyWipe();
    CheckSharedDepartureAndRoadStop();
    CheckSharedPonies();
    char error[256];
    CcSim *first = CcCoopCreate(UINT32_C(0xc0a71a9e));
    CcSim *second = CcCoopCreate(42U);
    CC_CHECK(first != NULL && second != NULL);
    uint64_t initial = CcSimHash(first);
    CC_CHECK(!CcCoopApply(first, "trade", 0U, CC_GOOD_BREAD, 1000000, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == initial);
    CC_CHECK(!CcCoopApply(first, "advance", 0U, 0, 1, error, sizeof(error)));
    CC_CHECK(!CcCoopApply(first, "fight", 0U, 0, 1, error, sizeof(error)));
    CC_CHECK(CcCoopApply(first, "trade", 0U, CC_GOOD_BREAD, 1, error, sizeof(error)));
    unsigned char *bytes = NULL;
    size_t length = 0U;
    CC_CHECK(CcCoopEncode(first, &bytes, &length, error, sizeof(error)));
    CC_CHECK(length > 100U);
    CC_CHECK(CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    bytes[0] = 'X';
    CC_CHECK(!CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    CcCoopFree(bytes);
    char *json = malloc(CC_COOP_JSON_CAPACITY);
    CC_CHECK(json != NULL);
    CC_CHECK(CcCoopSnapshot(first, json, CC_COOP_JSON_CAPACITY));
    CC_CHECK(strstr(json, "\"company\":{") != NULL);
    CC_CHECK(!CcCoopSnapshot(first, json, 2U));
    free(json);
    CC_CHECK(!CcCoopAdvance(first, -1, error, sizeof(error)));
    CC_CHECK(!CcCoopAdvance(first, 3601, error, sizeof(error)));
    initial = CcSimHash(first);
    CC_CHECK(!CcCoopAdvanceAway(first, -1, error, sizeof(error)));
    CC_CHECK(!CcCoopAdvanceAway(first, 366, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == initial);
    int32_t day = first->current_day;
    CcId location = first->player.location_id;
    for (int year = 0; year < 100; ++year)
        CC_CHECK(CcCoopAdvanceAway(first, 365, error, sizeof(error)));
    CC_CHECK(first->current_day == day + 36500);
    CC_CHECK(first->player.location_id == location);
    CC_CHECK(CcCoopEncode(first, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    CcCoopFree(bytes);
    CcCoopDestroy(first);
    CcCoopDestroy(second);
    return 0;
}
