#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <sqlite3.h>
#include <string.h>

static CcSim sim, restored;
static char error[256];

static void Apply(CcJournal *journal, CcCommandKind kind, int32_t pony, int32_t slot)
{
    CcCommand command = {.kind = kind, .target_id = (CcId)pony + 1U, .amount = slot};
    bool ok = journal ? CcJournalApply(journal, &sim, &command, error, sizeof(error)) :
                        CcSimApply(&sim, &command, error, sizeof(error));
    if (!ok) (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(ok);
}

int main(void)
{
    bool selected[CC_PONY_COUNT] = {false};
    for (uint32_t seed = 1; seed <= 128; ++seed) {
        CcSimInit(&sim, seed);
        CcSimInit(&restored, seed);
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        CC_CHECK(CcPoniesValidate(&sim));
        CC_CHECK(sim.pony_company.team[0] != sim.pony_company.team[1]);
        selected[sim.pony_company.team[0]] = true;
        selected[sim.pony_company.team[1]] = true;
    }
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) CC_CHECK(selected[i]);
    CcSimInit(&sim, 117U);
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = sim.settlements[1].id};
    CC_CHECK(CcSimApply(&sim, &travel, error, sizeof(error)));
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        if (sim.pony_company.ponies[i].route_id != 0U)
            sim.pony_company.ponies[i].route_id = sim.journey.route_id;
    }
    for (int32_t tick = 0; tick < 10000 && CcPonyOnRoad(&sim) < 0; ++tick)
        CcSimAdvanceRuntimeTicks(&sim, 1);
    int32_t pony = CcPonyOnRoad(&sim);
    CC_CHECK(pony >= 0);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    const char *path = "pony-loop-test.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    Apply(journal, CC_COMMAND_MEET_PONY, pony, 0);
    uint64_t paused = CcSimHash(&sim);
    CcSimAdvanceRuntimeTicks(&sim, 300);
    CC_CHECK(CcSimHash(&sim) == paused);
    CcCommand swap = {.kind = CC_COMMAND_SWAP_PONY, .target_id = (CcId)pony + 1U};
    CC_CHECK(!CcSimApply(&sim, &swap, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == paused);
    CC_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == paused);
    CcJournalAbandon(&journal);

    CcPony *p = &sim.pony_company.ponies[pony];
    CcGood good = CcPonyQuestGood(p);
    sim.player.cargo[good] = 0;
    CcCommand help = {.kind = CC_COMMAND_HELP_PONY, .target_id = (CcId)pony + 1U};
    paused = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &help, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == paused);
    sim.player.cargo[good] = p->quest_amount;
    journal = CcJournalRestart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    Apply(journal, CC_COMMAND_HELP_PONY, pony, 0);
    CC_CHECK(p->ready && p->bond == 1 && p->quests_completed == 1);
    paused = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &help, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == paused);
    int32_t old = sim.pony_company.team[0];
    int32_t old_quest = sim.pony_company.ponies[old].quest_kind;
    sim.horse_team[0].fatigue = 24;
    CC_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)));
    Apply(journal, CC_COMMAND_SWAP_PONY, pony, 0);
    CC_CHECK(sim.pony_company.team[0] == pony);
    CC_CHECK(sim.pony_company.ponies[old].releases == 1);
    CC_CHECK(sim.pony_company.ponies[old].fatigue == 24);
    CC_CHECK(sim.pony_company.ponies[old].quest_kind != old_quest);
    CC_CHECK(sim.pony_company.ponies[old].route_id != sim.journey.route_id);
    CC_CHECK(sim.pony_company.ponies[old].route_id != 0U);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    uint64_t expected = CcSimHash(&sim);
    CC_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected);

    /* A later reunion keeps history and asks for the generated next quest. */
    sim.pony_company.ponies[old].route_id = sim.journey.route_id;
    sim.pony_company.ponies[old].last_met_day = 0;
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i)
        if (i != old) sim.pony_company.ponies[i].last_met_day = sim.current_day;
    Apply(NULL, CC_COMMAND_MEET_PONY, old, 0);
    CC_CHECK(sim.pony_company.ponies[old].bond == 1);
    CC_CHECK(!sim.pony_company.ponies[old].ready);
    Apply(NULL, CC_COMMAND_LEAVE_PONY, old, 0);
    CC_CHECK(CcPonyOnRoad(&sim) == -1);

    /* Existing campaigns gain a pair after their original hash is checked. */
    const uint32_t legacy_versions[] = {37U, 39U};
    for (int32_t version = 0; version < 2; ++version) {
        CcSimInit(&sim, 55U);
        sim.schema_version = legacy_versions[version];
        CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
        CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
        CC_CHECK(restored.schema_version == 40U && CcPoniesValidate(&restored));
    }
    CC_CHECK(CcSaveWrite(path, &restored, error, sizeof(error)));
    sqlite3 *db = NULL;
    CC_CHECK(sqlite3_open(path, &db) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(db, "DELETE FROM rainbow_pony WHERE id=6;", NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(db) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &sim, error, sizeof(error)));
    (void)remove(path);
    (void)puts("PASS rainbow pony assignment, quests, swaps, reunion, save, replay, migration, and corruption checks");
    return 0;
}
