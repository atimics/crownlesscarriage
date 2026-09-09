#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before, restored;
static char error[256];

static void Valid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}

static void HistoricalSnapshot(void)
{
    CcSimInit(&sim, UINT32_C(0x9e3779b9));
    sim.schema_version = 73U;
    for (int day = 0; day < 12410; ++day) CcSimAdvanceDays(&sim, 1);
    bool retired = false;
    for (int i = 0; i < sim.situation_count; ++i) {
        const CcSituation *quest = &sim.situations[i];
        retired |= quest->status != CC_SITUATION_ACTIVE &&
            (CcSimCharacter(&sim, quest->sponsor_character_id) == NULL ||
             CcSimCharacter(&sim, quest->affected_character_id) == NULL);
    }
    CC_CHECK(retired);
    Valid();
}

static void CheckHistoricalSave(void)
{
    const char *fixture = "quest-history-fixture.ccsave";
    FILE *input = fopen(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-73-retired-cast.ccsave", "rb");
    FILE *output = fopen(fixture, "wb");
    CC_CHECK(input != NULL && output != NULL);
    unsigned char buffer[8192]; size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0)
        CC_CHECK(fwrite(buffer, 1, count, output) == count);
    CC_CHECK(ferror(input) == 0);
    CC_CHECK(fclose(input) == 0 && fclose(output) == 0);
    HistoricalSnapshot();
    before = sim; before.schema_version = CC_SIM_SCHEMA_VERSION;
    CC_CHECK(CcSaveRead(fixture, &sim, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&before));
    CC_CHECK(sim.player.coins == before.player.coins);
    for (int i = 0; i < sim.situation_count; ++i) {
        CC_CHECK(sim.situations[i].sponsor_character_id == before.situations[i].sponsor_character_id);
        CC_CHECK(sim.situations[i].affected_character_id == before.situations[i].affected_character_id);
        CC_CHECK(strcmp(sim.situations[i].sponsor_name, before.situations[i].sponsor_name) == 0);
        CC_CHECK(strcmp(sim.situations[i].affected_name, before.situations[i].affected_name) == 0);
    }
    CC_CHECK(sim.current_day == 12411 && sim.schema_version == CC_SIM_SCHEMA_VERSION);
    Valid(); RoundTrip();
    (void)remove(fixture);
    before = sim;
    /* A completed record accepts its issued ID; an active offer needs its cast. */
    int slot = -1;
    for (int i = 0; i < sim.situation_count; ++i)
        if (sim.situations[i].status != CC_SITUATION_ACTIVE &&
            (CcSimCharacter(&sim, sim.situations[i].sponsor_character_id) == NULL ||
             CcSimCharacter(&sim, sim.situations[i].affected_character_id) == NULL)) slot = i;
    CC_CHECK(slot >= 0);
    sim.schema_version = 73U;
    sim.situations[slot].status = CC_SITUATION_ACTIVE;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.schema_version = 73U;
    sim.situations[slot].sponsor_character_id = CcMakeId(CC_ENTITY_CHARACTER, sim.next_entity_serial);
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.schema_version = 73U;
    sim.situations[slot].sponsor_character_id = sim.settlements[0].id;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before;
    const char *path = "quest-history-restart.ccsave";
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
    (void)remove("quest-history-restart.ccsave-wal");
    (void)remove("quest-history-restart.ccsave-shm");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--write-history-fixture") == 0) {
        HistoricalSnapshot();
        unsigned char *bytes = NULL; size_t length = 0;
        CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
        FILE *file = fopen(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-73-retired-cast.ccsave", "wb");
        CC_CHECK(file != NULL && fwrite(bytes, 1, length, file) == length);
        CC_CHECK(fclose(file) == 0);
        CcSaveFreeBuffer(bytes);
        return 0;
    }
    CheckHistoricalSave();
    CcSimInit(&sim, 42U);
    sim.current_day = 2;
    /* Existing casts retain their identities even when their lives move on. */
    CcCharacter *person = &sim.characters[0];
    person->current_settlement_id = sim.settlements[5].id;
    person->role = CC_CHARACTER_TRAVELLER;
    person->goal = CC_CHARACTER_GOAL_CARRY_NEWS;
    person->faction_id = sim.factions[5].id;
    person->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    before = sim;
    CcSimInitializeCharacters(&sim);
    for (int i = 0; i < sim.character_count; ++i) {
        CC_CHECK(sim.characters[i].current_settlement_id == before.characters[i].current_settlement_id);
        CC_CHECK(sim.characters[i].role == before.characters[i].role);
        CC_CHECK(sim.characters[i].goal == before.characters[i].goal);
        CC_CHECK(sim.characters[i].faction_id == before.characters[i].faction_id);
    }
    CC_CHECK(person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING);
    Valid();
    RoundTrip();
    /* A local death with every possible adult elsewhere closes the commission. */
    CcSimInit(&sim, 42U);
    CcId quest_id = sim.situations[0].id;
    CcId dead_id = sim.situations[0].sponsor_character_id;
    CcId offer = CcSimSituationOfferSettlementId(&sim, &sim.situations[0]);
    CcId away = offer == sim.settlements[5].id ? sim.settlements[4].id : sim.settlements[5].id;
    for (int i = 0; i < sim.character_count; ++i) {
        sim.characters[i].current_settlement_id = away;
        if (sim.characters[i].id == dead_id) sim.characters[i].death_day = 2;
    }
    before = sim;
    CcSimAdvanceDays(&sim, 1);
    const CcSituation *quest = CcSimSituation(&sim, quest_id);
    CC_CHECK(quest != NULL && quest->status == CC_SITUATION_FAILED && quest->end_reason == CC_QUEST_END_INVALIDATED);
    CC_CHECK(quest->sponsor_character_id == dead_id);
    CC_CHECK(CcSimQuestOutcome(&sim, quest_id) != NULL);
    Valid();
    RoundTrip();
    sim = before;
    CcCharacter *replacement = NULL;
    for (int i = 0; i < sim.character_count; ++i) {
        CcCharacter *candidate = &sim.characters[i];
        if (candidate->id == dead_id || candidate->id == sim.situations[0].affected_character_id ||
            CcCharacterAgeYears(&sim, candidate) < 16) continue;
        replacement = candidate;
        break;
    }
    CC_CHECK(replacement != NULL);
    replacement->current_settlement_id = offer;
    replacement->role = CC_CHARACTER_TRAVELLER;
    replacement->goal = CC_CHARACTER_GOAL_CARRY_NEWS;
    replacement->faction_id = 0U;
    replacement->activity = CC_CHARACTER_ACTIVITY_WORKING;
    CcId replacement_id = replacement->id;
    CcSimAdvanceDays(&sim, 1);
    quest = CcSimSituation(&sim, quest_id);
    CC_CHECK(quest != NULL && quest->status == CC_SITUATION_ACTIVE && quest->sponsor_character_id == replacement_id);
    replacement = (CcCharacter *)CcSimCharacter(&sim, replacement_id);
    CC_CHECK(replacement->role == CC_CHARACTER_TRAVELLER && replacement->goal == CC_CHARACTER_GOAL_CARRY_NEWS && replacement->faction_id == 0U);
    Valid();
    RoundTrip();
    for (int unavailable = 0; unavailable < 3; ++unavailable) {
        sim = before;
        replacement = (CcCharacter *)CcSimCharacter(&sim, replacement_id);
        replacement->current_settlement_id = offer;
        replacement->faction_id = 0U;
        if (unavailable == 0) replacement->bandit_group_id = sim.bandits[0].id;
        if (unavailable == 1) replacement->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
        if (unavailable == 2) replacement->birth_day = sim.current_day - 10 * 365;
        CcSimAdvanceDays(&sim, 1);
        quest = CcSimSituation(&sim, quest_id);
        CC_CHECK(quest != NULL && quest->status == CC_SITUATION_FAILED);
        Valid();
    }
    /* Replaying a current journal keeps the same cast and history. */
    CcSimInit(&sim, 42U);
    const char *path = "quest-cast-test.ccsave";
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 100, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
    (void)remove("quest-cast-test.ccsave-wal");
    (void)remove("quest-cast-test.ccsave-shm");
    /* Schema 73 verifies with its old rules before adopting the new rules. */
    CcSimInit(&sim, 42U);
    sim.schema_version = 73U;
    CcSimAdvanceDays(&sim, 100);
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    sim.schema_version = CC_SIM_SCHEMA_VERSION;
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    FILE *source = fopen(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-73-generator-25-cast-journal.ccsave", "rb");
    FILE *copy = fopen(path, "wb");
    CC_CHECK(source != NULL && copy != NULL);
    unsigned char buffer[8192];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), source)) > 0)
        CC_CHECK(fwrite(buffer, 1, count, copy) == count);
    CC_CHECK(ferror(source) == 0);
    CC_CHECK(fclose(source) == 0 && fclose(copy) == 0);
    journal = CcJournalResume(path, &sim, error, sizeof(error));
    if (journal == NULL) fprintf(stderr, "%s\n", error);
    CC_CHECK(journal != NULL && sim.schema_version == CC_SIM_SCHEMA_VERSION);
    sim.schema_version = 73U;
    CC_CHECK(CcSimHash(&sim) == UINT64_C(17607823286729841219));
    sim.schema_version = CC_SIM_SCHEMA_VERSION;
    Valid();
    CC_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)));
    (void)remove(path);
    (void)remove("quest-cast-test.ccsave-wal");
    (void)remove("quest-cast-test.ccsave-shm");
    puts("Quest casting, closure, save migration, and replay passed.");
    return 0;
}
