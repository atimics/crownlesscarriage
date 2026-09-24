#include "sim/cc_scriven.h"
#include "sim/cc_sim.h"
#include "sim/cc_archive_staff.h"

#include <stdlib.h>

int32_t CcSimCharacterRecordCapacity(const CcSim *sim)
{
    return sim != NULL && sim->schema_version >= 101U ?
        CC_MAX_CHARACTER_RECORDS : CC_MAX_CHARACTERS;
}

bool CcSimCharacterIsActive(const CcSim *sim, const CcCharacter *person)
{
    return sim != NULL && person != NULL &&
        (sim->schema_version < 101U || person->detail_active);
}

int32_t CcSimActiveCharacterCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    if (sim->schema_version < 101U) return sim->character_count;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->character_count && i < CC_MAX_CHARACTER_RECORDS; ++i)
        if (sim->characters[i].detail_active) ++count;
    return count;
}

static bool ProtectedPerson(const CcSim *sim, const CcCharacter *person)
{
    if (CcScrivenTravelling(sim, person->id)) return true;
    if (person->travel_destination_id != 0U || person->bandit_group_id != 0U ||
        person->current_settlement_id == sim->player.location_id ||
        person->id == sim->archives.abbot_character_id ||
        person->id == sim->dragon_campaign.hero_character_id ||
        person->id == sim->dragon_campaign.patron_character_id) return true;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *realm = &sim->kingdoms[i];
        if (realm->ruler_character_id == person->id ||
            realm->monastery_patron_id == person->id ||
            realm->anointed_by_character_id == person->id) return true;
    }
    if (sim->archive_recruitment.status > 0 &&
        (person->id == sim->archive_recruitment.person_id ||
         person->id == sim->archive_recruitment.trainer_id)) return true;
    if (CcSimArchiveStaffMember(sim, person->id)) return true;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *task = &sim->situations[i];
        if (task->status != CC_SITUATION_ACTIVE &&
            task->id != sim->player.accepted_situation_id) continue;
        if (task->sponsor_character_id == person->id ||
            task->affected_character_id == person->id ||
            task->witness_character_id == person->id) return true;
    }
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        const CcRelationship *tie = &sim->relationships[i];
        if (tie->obligation != 0 &&
            (tie->from_character_id == person->id || tie->to_character_id == person->id))
            return true;
    }
    return false;
}

typedef struct CastCandidate {
    CcId id;
    int32_t slot;
    int32_t priority;
    int32_t last_active_day;
} CastCandidate;

static int CompareCandidate(const void *left, const void *right)
{
    const CastCandidate *a = left, *b = right;
    if (a->priority != b->priority) return a->priority > b->priority ? -1 : 1;
    if (a->last_active_day != b->last_active_day)
        return a->last_active_day < b->last_active_day ? -1 : 1;
    return a->id < b->id ? -1 : a->id > b->id ? 1 : 0;
}

void CcSimRefreshActiveCast(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 101U ||
        sim->character_count < 0 || sim->character_count > CC_MAX_CHARACTER_RECORDS) return;
    if (sim->character_count <= CC_MAX_CHARACTERS) {
        for (int32_t i = 0; i < sim->character_count; ++i) {
            CcCharacter *person = &sim->characters[i];
            if (!person->detail_active) person->last_active_day = sim->current_day;
            person->detail_active = true;
        }
        return;
    }
    CastCandidate candidates[CC_MAX_CHARACTER_RECORDS];
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        bool pinned = ProtectedPerson(sim, person);
        /* Current commitments keep their places. New nearby people take the
           next available places, then recent contact and ordinary work. */
        int32_t days_away = sim->current_day - person->last_active_day;
        int32_t priority = pinned ? (person->detail_active ? 5 : 4) :
            person->detail_active ? (days_away < 7 ? 2 : 1) :
            sim->current_day - person->introduced_day < 14 ? 3 : 1;
        candidates[i] = (CastCandidate){person->id, i, priority, person->last_active_day};
    }
    qsort(candidates, (size_t)sim->character_count, sizeof(candidates[0]), CompareCandidate);
    for (int32_t rank = 0; rank < sim->character_count; ++rank) {
        CcCharacter *person = &sim->characters[candidates[rank].slot];
        bool active = rank < CC_MAX_CHARACTERS;
        if (active && !person->detail_active) person->last_active_day = sim->current_day;
        person->detail_active = active;
    }
}

bool CcSimActivateCharacter(CcSim *sim, CcId person_id)
{
    if (sim == NULL) return false;
    CcCharacter *person = (CcCharacter *)CcSimCharacter(sim, person_id);
    if (person == NULL) return false;
    if (sim->schema_version < 101U) return true;
    if (person->detail_active) {
        person->last_active_day = sim->current_day;
        return true;
    }
    CcCharacter *retiring = NULL;
    if (CcSimActiveCharacterCount(sim) >= CC_MAX_CHARACTERS) {
        for (int32_t i = 0; i < sim->character_count; ++i) {
            CcCharacter *candidate = &sim->characters[i];
            if (!candidate->detail_active || ProtectedPerson(sim, candidate)) continue;
            if (retiring == NULL || candidate->last_active_day < retiring->last_active_day ||
                (candidate->last_active_day == retiring->last_active_day && candidate->id < retiring->id))
                retiring = candidate;
        }
        if (retiring == NULL) return false;
    }
    if (retiring != NULL) retiring->detail_active = false;
    person->detail_active = true;
    person->last_active_day = sim->current_day;
    return true;
}
