#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>

static CcSim sim;
static CcSim restored;
static CcSim baseline;
static CcSim replay;
static CcSim field_baseline;
static bool check_hash = true;
static unsigned saved_fields;
static const char *saved_field = "lifecycle";

static void CheckValid(const CcSim *world)
{
    char error[256];
    if (!CcSimValidate(world, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", saved_field, error);
        CC_CHECK(false);
    }
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    char error[256];
    CheckValid(&sim);
    if (!CcSaveEncode(&sim, &bytes, &length, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", saved_field, error);
        CC_CHECK(false);
    }
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    if (check_hash) CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}

#define HASH_FIELD(field, value) do { \
    baseline = sim; \
    uint64_t hash_before = CcSimHash(&sim); \
    sim.field = (value); \
    if (check_hash) CC_CHECK(CcSimHash(&sim) != hash_before); \
    sim = baseline; \
} while (0)

/* Each valid mutation is checked directly after decode. The optional mode
   lets fault injection verify persistence independently of hashing. */
#define SAVED_FIELD(field, value) do { \
    sim = field_baseline; \
    saved_field = #field; \
    sim.field = (value); \
    CC_CHECK(sim.field != field_baseline.field); \
    if (check_hash) CC_CHECK(CcSimHash(&sim) != CcSimHash(&field_baseline)); \
    RoundTrip(); \
    if (restored.field != sim.field) { \
        fprintf(stderr, "Save round trip omitted %s\n", #field); \
        CC_CHECK(false); \
    } \
    saved_fields++; \
} while (0)

static void CheckSavedFields(int32_t listener, int32_t account, CcId retired_id)
{
    field_baseline = sim;
    /* Leave room to vary each date independently within a valid lifetime. */
    field_baseline.historic_characters[0].birth_day = sim.current_day - 3;
    field_baseline.historic_characters[0].death_day = sim.current_day - 1;
    CC_CHECK(CcSimHistoricCharacter(&sim, retired_id) == NULL);
    CC_CHECK(sim.historic_characters[0].generation > 0);
    SAVED_FIELD(historic_character_count, field_baseline.historic_character_count - 1);
    SAVED_FIELD(historic_characters[0].id, retired_id);
    SAVED_FIELD(historic_characters[0].name[0],
        field_baseline.historic_characters[0].name[0] == 'Z' ? 'Y' : 'Z');
    SAVED_FIELD(historic_characters[0].ancestor_id, retired_id);
    SAVED_FIELD(historic_characters[0].home_settlement_id,
        field_baseline.historic_characters[0].home_settlement_id == sim.settlements[0].id ?
            sim.settlements[1].id : sim.settlements[0].id);
    SAVED_FIELD(historic_characters[0].birth_day, field_baseline.historic_characters[0].birth_day - 1);
    SAVED_FIELD(historic_characters[0].death_day, field_baseline.historic_characters[0].death_day - 1);
    SAVED_FIELD(historic_characters[0].generation, field_baseline.historic_characters[0].generation + 1);
    SAVED_FIELD(historic_characters[0].role,
        (CcCharacterRole)(((int)field_baseline.historic_characters[0].role + 1) % (CC_CHARACTER_COURIER + 1)));
    SAVED_FIELD(historic_characters[0].importance, (field_baseline.historic_characters[0].importance + 1) % 3);
    SAVED_FIELD(characters[listener].knowledge[account].source_character_id, sim.characters[listener].id);
    SAVED_FIELD(characters[listener].knowledge[account].source_name[0],
        field_baseline.characters[listener].knowledge[account].source_name[0] == 'Z' ? 'Y' : 'Z');
    sim = field_baseline;
    printf("Verified %u independent lifetime and source fields\n", saved_fields);
}

int main(int argc, char **argv)
{
    CC_CHECK(argc == 1 || (argc == 2 && strcmp(argv[1], "--save-only") == 0));
    check_hash = argc == 1;
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    int32_t listener = -1, source = -1, account = -1;
    for (int32_t i = 0; i < sim.character_count && listener < 0; ++i) {
        for (int32_t k = 0; k < sim.characters[i].knowledge_count; ++k) {
            const CcCharacterKnowledge *item = &sim.characters[i].knowledge[k];
            if (item->kind != CC_KNOWLEDGE_PROBLEM_RUMOR) continue;
            for (int32_t j = 0; j < sim.character_count; ++j) {
                if (j != i && sim.characters[j].id == item->source_character_id) {
                    listener = i; source = j; account = k;
                    break;
                }
            }
            if (listener >= 0) break;
        }
    }
    CC_CHECK(listener >= 0 && source >= 0);
    CcCharacterKnowledge received = sim.characters[listener].knowledge[account];
    CcCharacter original = sim.characters[source];
    CC_CHECK(strcmp(received.source_name, original.name) == 0);
    for (int32_t i = 0; i < sim.character_count; ++i) {
        sim.characters[i].death_day = CC_SIM_MAX_DAY;
    }
    /* An existing told account comes from the ordinary mine social thread.
       Replace its teller through the same daily lifecycle used in play. */
    sim.characters[source].death_day = sim.current_day + 1;
    baseline = sim;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.characters[source].id != original.id);
    for (int32_t i = 0; i < sim.character_count; ++i) {
        if (i == source) continue;
        CC_CHECK(sim.characters[i].knowledge_count == baseline.characters[i].knowledge_count);
        CC_CHECK(memcmp(sim.characters[i].knowledge, baseline.characters[i].knowledge,
                        sizeof(sim.characters[i].knowledge)) == 0);
    }
    const CcHistoricCharacter *history = CcSimHistoricCharacter(&sim, original.id);
    CC_CHECK(history != NULL);
    CC_CHECK(strcmp(history->name, original.name) == 0);
    CC_CHECK(history->birth_day == original.birth_day);
    CC_CHECK(history->death_day == sim.current_day);
    CC_CHECK(history->home_settlement_id == original.home_settlement_id);
    CC_CHECK(history->role == original.role);
    CC_CHECK(history->ancestor_id == original.ancestor_id);
    CC_CHECK(history->generation == original.generation);
    CC_CHECK(memcmp(&sim.characters[listener].knowledge[account], &received,
                    sizeof(received)) == 0);
    CC_CHECK(sim.characters[source].knowledge_count == 0);
    RoundTrip();
    CC_CHECK(restored.historic_characters[0].id == original.id);
    CC_CHECK(restored.historic_characters[0].ancestor_id == original.ancestor_id);
    CC_CHECK(restored.historic_characters[0].home_settlement_id == original.home_settlement_id);
    CC_CHECK(restored.historic_characters[0].birth_day == original.birth_day);
    CC_CHECK(restored.historic_characters[0].death_day == sim.current_day);
    CC_CHECK(restored.historic_characters[0].generation == original.generation);
    CC_CHECK(restored.historic_characters[0].role == original.role);
    CC_CHECK(restored.historic_characters[0].importance == sim.historic_characters[0].importance);
    HASH_FIELD(historic_character_count, 0);
    HASH_FIELD(historic_characters[0].id, original.id + 1U);
    HASH_FIELD(historic_characters[0].ancestor_id, original.id);
    HASH_FIELD(historic_characters[0].home_settlement_id, sim.player.id);
    HASH_FIELD(historic_characters[0].birth_day, original.birth_day - 1);
    HASH_FIELD(historic_characters[0].death_day, sim.current_day - 1);
    HASH_FIELD(historic_characters[0].generation, original.generation + 1);
    HASH_FIELD(historic_characters[0].role, CC_CHARACTER_COURIER);
    HASH_FIELD(historic_characters[0].importance,
               (sim.historic_characters[0].importance + 1) % 3);
    CC_CHECK(strcmp(restored.historic_characters[0].name, original.name) == 0);
    CC_CHECK(strcmp(restored.characters[listener].knowledge[account].source_name,
                    original.name) == 0);
    uint64_t before = CcSimHash(&sim);
    sim.historic_characters[0].name[0] = 'Z';
    if (check_hash) CC_CHECK(CcSimHash(&sim) != before);
    CC_CHECK(strcmp(sim.characters[listener].knowledge[account].source_name, original.name) == 0);
    sim = restored;
    before = CcSimHash(&sim);
    sim.characters[listener].knowledge[account].source_name[0] = 'Z';
    if (check_hash) CC_CHECK(CcSimHash(&sim) != before);
    sim = restored;

    replay = sim;
    for (int32_t i = 0; i < CC_MAX_HISTORIC_CHARACTERS + 2; ++i) {
        CcId prior = sim.characters[source].id;
        sim.characters[source].death_day = sim.current_day + 1;
        CcSimAdvanceDays(&sim, 1);
        replay.characters[source].death_day = replay.current_day + 1;
        CcSimAdvanceDays(&replay, 1);
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&replay));
        CC_CHECK(sim.characters[source].id != prior);
        CC_CHECK(sim.characters[source].id != received.source_character_id);
        CC_CHECK(sim.characters[listener].knowledge[account].source_character_id == original.id);
        CC_CHECK(strcmp(sim.characters[listener].knowledge[account].source_name, original.name) == 0);
        CheckValid(&sim);
    }
    CC_CHECK(sim.historic_character_count == CC_MAX_HISTORIC_CHARACTERS);
    CC_CHECK(CcSimHistoricCharacter(&sim, original.id) == NULL);
    RoundTrip();
    CC_CHECK(restored.characters[listener].knowledge[account].source_character_id == original.id);
    CC_CHECK(strcmp(restored.characters[listener].knowledge[account].source_name, original.name) == 0);
    CheckSavedFields(listener, account, original.id);
    puts("Received accounts survive source death and history retirement");
    return 0;
}
