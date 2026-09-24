#include "multiplayer/cc_coop.h"
#include "multiplayer/cc_coop_commands.h"
#include "test_support.h"
#include "persistence/cc_save.h"
#include "sim/cc_archive_recruitment.h"
#include "sim/cc_road_position.h"
#include <stdlib.h>
#include <string.h>

static bool ChooseSharedRoadOnward(CcSim *sim, char *error, size_t capacity)
{
    CcRoadLegPreview previews[3];
    int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
    for (int32_t i = 0; i < count; ++i) {
        if (previews[i].direction == sim->journey.road_direction &&
            previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID) {
            return CcCoopApply(sim, "road_leg", previews[i].decision_token,
                               0, 0, error, capacity);
        }
    }
    return false;
}

static void CheckCommandRoundTrips(void)
{
    CcSim *direct = CcCoopCreate(42U);
    CcSim *shared = CcCoopCreate(42U);
    CC_CHECK(direct != NULL && shared != NULL);
    for (int32_t kind = 1; kind < (int32_t)CC_COMMAND_COUNT; ++kind) {
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
        uint64_t before = CcSimHash(shared);
        bool actual = CcCoopApply(shared, name, 0U, CC_GOOD_BREAD, 1,
                                  shared_error, sizeof(shared_error));
        if (kind == CC_COMMAND_MINE_CONTEST ||
            kind == CC_COMMAND_MINE_RESOLVE_CONTEST ||
            (kind >= CC_COMMAND_FOOD_RELIEF_PROPOSE &&
             kind <= CC_COMMAND_FOOD_RELIEF_EXECUTE)) {
            CC_CHECK(!actual && shared_error[0] != '\0');
            CC_CHECK(CcSimHash(shared) == before);
            continue;
        }
        CC_CHECK(actual == expected);
        CC_CHECK(strcmp(direct_error, shared_error) == 0);
        CC_CHECK(CcSimHash(direct) == CcSimHash(shared));
    }
    CcCoopDestroy(direct);
    CcCoopDestroy(shared);
}

static void CheckCurrentSharedCommandNames(void)
{
    CC_CHECK(strcmp(CcCoopActionName(CC_COMMAND_CHOOSE_ROAD_LEG),"road_leg")==0);
    CC_CHECK(strcmp(CcCoopActionName(CC_COMMAND_MINE_LEARN_LEAD),
                    "mine_learn_lead")==0);
    CC_CHECK(strcmp(CcCoopActionName(CC_COMMAND_MINE_REPORT_RETURN),
                    "mine_report_return")==0);
    CC_CHECK(strcmp(CcCoopActionName(CC_COMMAND_PICKUP_RELIEF_CRATE),
                    "pickup_relief_crate")==0);
    CC_CHECK(strcmp(CcCoopActionName(CC_COMMAND_STOW_RELIEF_CRATE),
                    "stow_relief_crate")==0);
    CC_CHECK(CcCoopActionName(CC_COMMAND_COUNT)[0]=='\0');
    for (int32_t kind=1;kind<(int32_t)CC_COMMAND_COUNT;++kind) {
        const char *name=CcCoopActionName((CcCommandKind)kind);
        CC_CHECK(name[0]!='\0');
        for (int32_t prior=1;prior<kind;++prior)
            CC_CHECK(strcmp(name,CcCoopActionName((CcCommandKind)prior))!=0);
    }
}

static void CheckArchiveRecruitment(void)
{
    char error[256];
    CcSim *sim = CcCoopCreate(42U);
    CC_CHECK(sim != NULL);
    CcArchiveRecruitmentPlan plan = CcSimArchiveRecruitmentPlan(sim);
    CC_CHECK(plan.gate == CC_ARCHIVE_RECRUIT_READY);
    sim->player.location_id = sim->carriage.location_id = plan.seat_id;
    CC_CHECK(CcCoopApply(sim, "reserve_archive_recruitment", plan.person_id,
        CC_GOOD_FOOD, 0, error, sizeof(error)));
    CC_CHECK(sim->archive_recruitment.person_id == plan.person_id);
    CC_CHECK(sim->archive_recruitment.purse == 50);
    CC_CHECK(CcCoopApply(sim, "cancel_archive_recruitment", plan.person_id,
        CC_GOOD_FOOD, 0, error, sizeof(error)));
    CC_CHECK(sim->archive_recruitment.status == 0);
    CcCoopDestroy(sim);
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
    /* The carriage draws one animal, so the second seat is empty and cannot
       be swapped into. The swap has to land in the seat that exists, and the
       bridge has to agree about it on both sides. */
    CC_CHECK(host->pony_company.team[1] == -1);
    CC_CHECK(!CcCoopApply(host, "swap_pony", target, 0, 1,
                          error, sizeof(error)));
    int32_t released = host->pony_company.team[0];
    CC_CHECK(CcCoopApply(host, "swap_pony", target, 0, 0, error, sizeof(error)));
    CC_CHECK(host->pony_company.team[0] == pony);
    CC_CHECK(host->pony_company.team[1] == -1);
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
    host->journey.ambush_pending = false;
    host->journey.encounter_triggered = true;
    const CcRoadSite *site = NULL;
    for (int32_t step = 0; step < 10000 && site == NULL; ++step) {
        site = CcSimJourneyRoadSiteStop(host);
        if (site != NULL) break;
        if (host->journey.road_waiting_choice) {
            CC_CHECK(ChooseSharedRoadOnward(host, error, sizeof(error)));
        } else {
            CC_CHECK(CcCoopAdvance(host, 1, error, sizeof(error)));
        }
    }
    CC_CHECK(site != NULL);
    CC_CHECK(CcSimJourneyRoadSiteStop(host) == site);
    uint64_t stopped = CcSimHash(host);
    CC_CHECK(!CcCoopApply(host, "skip_watch", 0U, 0, 0,
                          error, sizeof(error)));
    CC_CHECK(CcSimHash(host) == stopped);
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(guest) == stopped && CcSimJourneyRoadSiteStop(guest) != NULL);
    host->player.cargo[CC_GOOD_TOOLS] = 2;
    host->player.cargo[CC_GOOD_WOOD] = 1;
    CC_CHECK(CcCoopApply(host, "clear_road_site", site->id, 0, 0, error, sizeof(error)));
    CC_CHECK(site->accessible);
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    CC_CHECK(CcSimRoadSite(guest, site->id)->accessible);
    CC_CHECK(CcCoopApply(guest, "pass_road_site", site->id, 0, 0, error, sizeof(error)));
    CC_CHECK(CcCoopApply(host, "pass_road_site", site->id, 0, 0, error, sizeof(error)));
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    int32_t before = host->carriage.progress_milli;
    CC_CHECK(CcCoopAdvance(host, 60, error, sizeof(error)));
    CC_CHECK(host->carriage.progress_milli > before);
    CC_CHECK(CcCoopAdvance(host, 3600, error, sizeof(error)));
    CC_CHECK(!host->journey.active ||
        host->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ||
        host->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE);
    CC_CHECK(CcSimJourneyRoadSiteStop(host) != site);
    CcCoopDestroy(host);
    CcCoopDestroy(guest);
}

static void CheckJourneyQuestRetirement(void)
{
    for (int stage = 0; stage < 3; ++stage) {
        char error[256];
        CcSim *sim = CcCoopCreate(42U);
        CcSim *restored = CcCoopCreate(42U);
        CC_CHECK(sim != NULL && restored != NULL);
        CcSituation *offer = NULL;
        for (int32_t i = 0; i < sim->situation_count; ++i) {
            if (sim->situations[i].kind == CC_SITUATION_RELIEF_DELIVERY &&
                sim->situations[i].status == CC_SITUATION_ACTIVE) {
                offer = &sim->situations[i];
                break;
            }
        }
        CC_CHECK(offer != NULL);
        CcId situation_id = offer->id;
        offer->target_id = sim->settlements[1].id;
        if (!CcCoopApply(sim, "accept", situation_id,
                         0, 0, error, sizeof(error))) {
            fprintf(stderr, "Journey retirement fixture: %s\n", error);
            CC_CHECK(false);
        }
        while (CcSimReliefCratesToLoad(offer) > 0) {
            CC_CHECK(CcCoopApply(sim, "pickup_relief_crate", situation_id,
                                0, 0, error, sizeof(error)));
            CC_CHECK(CcCoopApply(sim, "stow_relief_crate", situation_id,
                                0, 0, error, sizeof(error)));
        }
        sim->routes[0].closed = true;
        sim->bandits[0].route_id = sim->routes[0].id;
        CC_CHECK(CcCoopApply(sim, "travel", offer->target_id,
                            0, 0, error, sizeof(error)));
        for (int tick = 0; tick < 2000 &&
             sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED; ++tick) {
            const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
            if (site != NULL) {
                CC_CHECK(CcCoopApply(sim, "pass_road_site", site->id,
                                    0, 0, error, sizeof(error)));
            } else if (sim->journey.road_waiting_choice) {
                CC_CHECK(ChooseSharedRoadOnward(sim, error, sizeof(error)));
            } else {
                CC_CHECK(CcCoopAdvance(sim, 1, error, sizeof(error)));
            }
        }
        CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED);
        if (stage > 0) {
            CC_CHECK(CcCoopApply(sim, "negotiate", 0U,
                                0, 0, error, sizeof(error)));
            CC_CHECK(sim->resolved_journey_situation_id == situation_id);
            CC_CHECK(sim->resolved_journey_outcome ==
                     CC_JOURNEY_OUTCOME_NEGOTIATED);
        }
        if (stage == 2) {
            for (int step = 0; step < 10000 && sim->journey.active; ++step) {
                const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
                if (site != NULL) {
                    CC_CHECK(CcCoopApply(sim, "pass_road_site", site->id,
                                        0, 0, error, sizeof(error)));
                } else if (sim->journey.road_waiting_choice) {
                    CC_CHECK(ChooseSharedRoadOnward(
                        sim, error, sizeof(error)));
                } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
                    const char *action = CcSimJourneyStop(sim) ==
                        CC_JOURNEY_STOP_MIDDAY ? "break" : "camp";
                    CC_CHECK(CcCoopApply(sim, action, 0U,
                                        0, 0, error, sizeof(error)));
                } else {
                    CC_CHECK(CcCoopAdvance(sim, 60, error, sizeof(error)));
                }
            }
            CC_CHECK(!sim->journey.active);
            CC_CHECK(sim->resolved_journey_situation_id == situation_id);
            CC_CHECK(CcCoopApply(sim, "trade", 0U, (int32_t)offer->good,
                                -offer->quantity, error, sizeof(error)));
            CC_CHECK(offer->status == CC_SITUATION_RESOLVED);
        }
        CcJourneyEncounter journey = sim->journey;
        CcCarriageState carriage = sim->carriage;
        unsigned char *bytes = NULL;
        size_t length = 0;
        CC_CHECK(CcCoopEncode(sim, &bytes, &length, error, sizeof(error)));
        CC_CHECK(CcCoopDecode(restored, bytes, length, error, sizeof(error)));
        CcCoopFree(bytes);
        CC_CHECK(CcSimHash(sim) == CcSimHash(restored));
        for (int day = 0; day < 8 * 365 &&
             CcSimSituation(sim, situation_id) != NULL; ++day) {
            CC_CHECK(CcCoopAdvanceAway(sim, 1, error, sizeof(error)));
        }
        CC_CHECK(CcSimSituation(sim, situation_id) == NULL);
        CC_CHECK(CcSimQuestOutcome(sim, situation_id) != NULL);
        CC_CHECK(sim->journey.situation_id == 0U);
        CC_CHECK(sim->resolved_journey_situation_id == 0U);
        CC_CHECK(sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_NONE);
        CC_CHECK(sim->journey.active == journey.active);
        CC_CHECK(sim->journey.phase == journey.phase);
        CC_CHECK(sim->journey.route_id == journey.route_id);
        CC_CHECK(sim->journey.destination_id == journey.destination_id);
        CC_CHECK(sim->journey.elapsed_subticks == journey.elapsed_subticks);
        CC_CHECK(sim->carriage.mode == carriage.mode);
        CC_CHECK(sim->carriage.location_id == carriage.location_id);
        CC_CHECK(sim->carriage.progress_milli == carriage.progress_milli);
        int32_t elapsed = sim->current_day - restored->current_day;
        while (elapsed > 0) {
            int32_t days = elapsed < 365 ? elapsed : 365;
            CC_CHECK(CcCoopAdvanceAway(restored, days, error, sizeof(error)));
            elapsed -= days;
        }
        CC_CHECK(CcSimHash(sim) == CcSimHash(restored));
        CC_CHECK(CcCoopEncode(sim, &bytes, &length, error, sizeof(error)));
        CC_CHECK(CcCoopDecode(restored, bytes, length, error, sizeof(error)));
        CcCoopFree(bytes);
        CC_CHECK(CcSimHash(sim) == CcSimHash(restored));
        CcCoopDestroy(sim);
        CcCoopDestroy(restored);
    }
}

static void CheckTravelHoldClock(void)
{
    char error[256];
    CcSim *normal = CcCoopCreate(117U), *fast = CcCoopCreate(117U);
    CC_CHECK(normal != NULL && fast != NULL);
    CC_CHECK(CcCoopApply(normal, "travel", normal->settlements[1].id, 0, 0, error, sizeof(error)));
    normal->journey.ambush_pending = false;
    normal->journey.encounter_triggered = true;
    for (int scenario = 0; scenario < 3; ++scenario) {
        int target = scenario == 0 ? 0 : scenario == 1 ? 250 : 910;
        int attempts = 0;
        while (normal->journey.active &&
               normal->carriage.progress_milli < target && ++attempts < 10000) {
            if (normal->journey.road_waiting_choice) {
                CC_CHECK(ChooseSharedRoadOnward(
                    normal, error, sizeof(error)));
            } else if (CcSimJourneyRequiresRoadChoice(normal)) {
                const CcRoadSite *site = CcSimJourneyRoadSiteStop(normal);
                CC_CHECK(site != NULL);
                CC_CHECK(CcCoopApply(normal, "pass_road_site", site->id, 0, 0,
                    error, sizeof(error)));
            } else {
                CC_CHECK(CcCoopAdvance(normal, 1, error, sizeof(error)));
            }
        }
        if (attempts >= 10000) fprintf(stderr,"travel clock stalled phase%d progress%d target%d waiting%d pony%d\n",normal->journey.phase,normal->carriage.progress_milli,target,normal->journey.road_waiting_choice,normal->pony_company.encounter);
        CC_CHECK(attempts < 10000 && normal->journey.active);
        *fast = *normal;
        CC_CHECK(CcCoopAdvanceTravel(fast, 1, 8, error, sizeof(error)));
        CC_CHECK(CcCoopAdvance(normal, 8, error, sizeof(error)));
        CC_CHECK(CcSimHash(normal) == CcSimHash(fast));
    }
    CC_CHECK(!CcCoopAdvanceTravel(fast, 1, 9, error, sizeof(error)));
    CcCoopDestroy(normal); CcCoopDestroy(fast);
}

static void CheckTownAndTeamFacts(void)
{
    CcSim *sim = CcCoopCreate(42U);
    CC_CHECK(sim != NULL);
    CcSettlement *town = CcSimSettlementMutable(sim,
                                                  sim->player.location_id);
    CC_CHECK(town != NULL);
    town->population = 0;
    town->hunger = 100;
    town->service_mask = 0U;
    memset(town->stock, 0, sizeof(town->stock));
    for (int32_t i = 0; i < sim->character_count; ++i)
        sim->characters[i].current_settlement_id = sim->settlements[1].id;
    CcCharacter *visitor = &sim->characters[0];
    visitor->home_settlement_id = sim->settlements[1].id;
    visitor->current_settlement_id = town->id;
    visitor->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
    visitor->birth_day = sim->current_day - 30 * 365;
    visitor->death_day = 0;
    sim->carriage.condition = 0;
    sim->horse_team[0].health = 1;
    sim->horse_team[0].hunger = 100;
    uint64_t before = CcSimHash(sim);
    char *json = malloc(CC_COOP_JSON_CAPACITY);
    CC_CHECK(json != NULL);
    CC_CHECK(CcCoopSnapshot(sim, json, CC_COOP_JSON_CAPACITY));
    CC_CHECK(strstr(json, "\"residents\":0,\"visitors\":1,\"abandoned\":true") != NULL);
    CC_CHECK(strstr(json, "\"civilian_food_rations\":0,\"animal_feed_rations\":0,\"services\":[]") != NULL);
    CC_CHECK(strstr(json, "\"carriage_condition\":0") != NULL);
    CC_CHECK(strstr(json, "\"health\":1,\"hunger\":100") != NULL);
    CC_CHECK(CcSimHash(sim) == before);

    town->population = 180;
    town->hunger = 70;
    town->service_mask = UINT32_C(1) << CC_SERVICE_STABLE;
    town->stock[CC_GOOD_BREAD] = 8;
    CC_CHECK(CcCoopSnapshot(sim, json, CC_COOP_JSON_CAPACITY));
    CC_CHECK(strstr(json, "\"residents\":180,\"visitors\":1,\"abandoned\":false") != NULL);
    CC_CHECK(strstr(json, "\"services\":[\"Stable\"]") != NULL);
    free(json);
    CcCoopDestroy(sim);
}

int main(void)
{
    CheckTownAndTeamFacts();
    CheckTravelHoldClock();
    CheckCommandRoundTrips();
    CheckCurrentSharedCommandNames();
    CheckArchiveRecruitment();
    CheckJourneyQuestRetirement();
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
