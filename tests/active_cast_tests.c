#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static void Visit(CcSim *sim, int slot)
{
    sim->player.location_id = sim->settlements[slot].id;
    sim->carriage.location_id = sim->player.location_id;
    CcMoney gold = CcSimTrackedGold(sim);
    int32_t population = sim->settlements[slot].population;
    int32_t stocks[CC_GOOD_COUNT];
    memcpy(stocks, sim->settlements[slot].stock, sizeof(stocks));
    CcSimPeopleEnterSettlement(sim);
    CC_CHECK(CcSimActiveCharacterCount(sim) <= CC_MAX_CHARACTERS);
    CC_CHECK(CcSimTrackedGold(sim) == gold);
    CC_CHECK(sim->settlements[slot].population == population);
    CC_CHECK(memcmp(stocks, sim->settlements[slot].stock, sizeof(stocks)) == 0);
}

static void CheckPersistentPeople(void)
{
    static CcSim sim, restored;
    char error[256];
    const char *path = "active-cast.ccsave";
    CcSimInit(&sim, 42U);
    CC_CHECK(CcSimActiveCharacterCount(&sim) == sim.character_count);
    Visit(&sim, 0);
    CcCharacter *first = NULL;
    for (int32_t i = sim.character_count - 1; i >= 0; --i) {
        CcCharacter *person = &sim.characters[i];
        if (person->current_settlement_id == sim.player.location_id &&
            person->role == CC_CHARACTER_LABORER) { first = person; break; }
    }
    CC_CHECK(first != NULL);
    CcId id = first->id;
    uint32_t appearance = first->appearance_seed;
    char name[CC_NAME_CAPACITY];
    (void)snprintf(name, sizeof(name), "%s", first->name);
    CC_CHECK(first->introduced_day == 0);
    CcCommand talk = {.kind = CC_COMMAND_EXCHANGE_GOSSIP, .target_id = id};
    CC_CHECK(CcSimApply(&sim, &talk, error, sizeof(error)));
    CC_CHECK(CcSimCharacter(&sim, id)->introduced_day == sim.current_day);
    for (int town = 1; town < sim.settlement_count; ++town) Visit(&sim, town);
    CC_CHECK(sim.character_count > CC_MAX_CHARACTERS);
    CC_CHECK(sim.character_count <= CC_MAX_CHARACTER_RECORDS);
    CC_CHECK(CcSimActiveCharacterCount(&sim) == CC_MAX_CHARACTERS);
    /* Let the recent-contact protection end, then make ordinary people rotate. */
    CcSimAdvanceDays(&sim, 21);
    first = (CcCharacter *)CcSimCharacter(&sim, id);
    CC_CHECK(first != NULL);
    /* Ordinary residents leave the active cast as other towns take its places. */
    CC_CHECK(!first->detail_active);
    CC_CHECK(strcmp(first->name, name) == 0);
    CC_CHECK(first->appearance_seed == appearance);
    CC_CHECK(first->introduced_day == 1);
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcSimCharacter(&sim, id) == first);
    CC_CHECK(CcSimHash(&sim) == hash);
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    const CcCharacter *loaded = CcSimCharacter(&restored, id);
    CC_CHECK(loaded != NULL && !loaded->detail_active && loaded->introduced_day == 1);
    CC_CHECK(strcmp(loaded->name, name) == 0 && loaded->appearance_seed == appearance);
    CC_CHECK(loaded->travel_coins == first->travel_coins);
    CC_CHECK(loaded->memory_count == first->memory_count);
    CC_CHECK(memcmp(loaded->memories, first->memories, sizeof(first->memories)) == 0);
    CC_CHECK(loaded->knowledge_count == first->knowledge_count);
    CC_CHECK(memcmp(loaded->knowledge, first->knowledge, sizeof(first->knowledge)) == 0);
    CC_CHECK(restored.relationship_count == sim.relationship_count);
    CC_CHECK(memcmp(restored.relationships, sim.relationships,
                   (size_t)sim.relationship_count * sizeof(sim.relationships[0])) == 0);
    Visit(&sim, 0); Visit(&restored, 0);
    CC_CHECK(CcSimActivateCharacter(&sim, id));
    CC_CHECK(CcSimActivateCharacter(&restored, id));
    CC_CHECK(CcSimCharacter(&sim, id)->detail_active);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    /* Every persisted field independently affects the hash. */
    uint64_t active_hash = CcSimHash(&restored);
    CcCharacter *changed = (CcCharacter *)CcSimCharacter(&restored, id);
    changed->detail_active = false; CC_CHECK(CcSimHash(&restored) != active_hash);
    changed->detail_active = true; changed->last_active_day -= 1;
    CC_CHECK(CcSimHash(&restored) != active_hash);
    changed->last_active_day += 1; changed->introduced_day += 1;
    CC_CHECK(CcSimHash(&restored) != active_hash);
    changed->introduced_day -= 1;
    CC_CHECK(CcSimHash(&restored) == active_hash);
    /* A dated death still runs while the person is inactive. */
    Visit(&sim, 5);
    first = (CcCharacter *)CcSimCharacter(&sim, id);
    first->detail_active = false;
    first->death_day = sim.current_day + 1;
    CcSimAdvanceDays(&sim, 1);
    const CcHistoricCharacter *past = CcSimHistoricCharacter(&sim, id);
    CC_CHECK(past != NULL && strcmp(past->name, name) == 0);
    CC_CHECK(CcSimCharacter(&sim, id) == NULL);
    CC_CHECK(CcSimActiveCharacterCount(&sim) <= CC_MAX_CHARACTERS);
    (void)remove(path);
    (void)remove("active-cast.ccsave-wal");
    (void)remove("active-cast.ccsave-shm");
    printf("people=%d active=%d sim_bytes=%zu person_bytes=%zu\n",
        sim.character_count, CcSimActiveCharacterCount(&sim), sizeof(CcSim), sizeof(CcCharacter));
}

static void CheckProtectedCapacity(void)
{
    static CcSim sim;
    CcSimInit(&sim, 42U);
    for (int town = 0; town < sim.settlement_count; ++town) Visit(&sim, town);
    CcId inactive = 0;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        CcCharacter *person = &sim.characters[i];
        if (!person->detail_active) { inactive = person->id; continue; }
        person->travel_destination_id = sim.settlements[0].id;
        person->travel_arrival_day = sim.current_day + 3;
    }
    CC_CHECK(inactive != 0);
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcSimActivateCharacter(&sim, inactive));
    CC_CHECK(CcSimHash(&sim) == hash);
    CC_CHECK(CcSimActiveCharacterCount(&sim) == CC_MAX_CHARACTERS);
    /* Old versions still use the original storage and hash rules. */
    sim.schema_version = 99U;
    CC_CHECK(CcSimCharacterRecordCapacity(&sim) == 128);
    CC_CHECK(CcSimGossipCarrierCapacity(&sim) == CC_LEGACY_GOSSIP_CARRIERS + 128);
    sim.schema_version = 81U;
    CC_CHECK(CcSimGossipCarrierCapacity(&sim) == CC_LEGACY_GOSSIP_CARRIERS + CC_LEGACY_CHARACTER_CAP);
}

static void CheckNewCompanyIntroductions(void)
{
    static CcSim sim, restored;
    const char *path = "new-company-names.ccsave";
    char error[256];
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CC_CHECK(sim.schema_version == 110U);
    CcId company = sim.player.id;
    CcId introduced_id = 0U;
    for (int32_t i = 0; i < sim.character_count; ++i)
        if (strcmp(sim.characters[i].name, "Mara Venn") == 0)
            introduced_id = sim.characters[i].id;
    CC_CHECK(introduced_id != 0U);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand talk = {.kind = CC_COMMAND_EXCHANGE_GOSSIP, .target_id = introduced_id};
    CC_CHECK(CcJournalApply(journal, &sim, &talk, error, sizeof(error)));
    CC_CHECK(CcSimCharacter(&sim, introduced_id)->introduced_day == 1);
    CcCommand wipe = {.kind = CC_COMMAND_PARTY_WIPE,
                      .target_id = (CcId)sim.current_day};
    CC_CHECK(CcJournalApply(journal, &sim, &wipe, error, sizeof(error)));
    CC_CHECK(sim.player.id == company);
    const CcCharacter *survivor = CcSimCharacter(&sim, introduced_id);
    CC_CHECK(survivor != NULL && survivor->introduced_day == 0);
    for (int32_t i = 0; i < sim.character_count; ++i)
        CC_CHECK(sim.characters[i].introduced_day == 0);
    uint64_t wiped_hash = CcSimHash(&sim);
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == wiped_hash);
    CC_CHECK(CcSimCharacter(&restored, introduced_id)->introduced_day == 0);
    CcId next_id = 0U;
    for (int32_t i = 0; i < restored.character_count; ++i) {
        const CcCharacter *person = &restored.characters[i];
        if (person->current_settlement_id == restored.player.location_id &&
            person->activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
            CcCharacterAgeYears(&restored, person) >= 16) {
            next_id = person->id;
            break;
        }
    }
    CC_CHECK(next_id != 0U);
    talk.target_id = next_id;
    CC_CHECK(CcJournalApply(journal, &restored, &talk, error, sizeof(error)));
    CC_CHECK(CcSimCharacter(&restored, next_id)->introduced_day == restored.current_day);
    uint64_t learned_hash = CcSimHash(&restored);
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == learned_hash);
    CC_CHECK(CcSimCharacter(&sim, next_id)->introduced_day == sim.current_day);
    CcJournalAbandon(&journal);
    (void)remove(path);
    (void)remove("new-company-names.ccsave-wal");
    (void)remove("new-company-names.ccsave-shm");
}

int main(void)
{
    CheckPersistentPeople();
    CheckProtectedCapacity();
    CheckNewCompanyIntroductions();
    return 0;
}
