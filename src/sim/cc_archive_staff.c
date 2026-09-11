#include "sim/cc_archive_staff.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_identity_internal.h"

bool CcSimArchiveStaffMember(const CcSim *sim, CcId person_id)
{
    if (sim == NULL || sim->schema_version < 85U || !sim->archive_staff.active || person_id == 0) return false;
    for (int32_t i = 0; i < CC_MAX_SCRIBES; ++i)
        if (sim->archive_staff.person_ids[i] == person_id) return true;
    return false;
}
bool CcSimArchiveStaffWorking(const CcSim *sim, CcId person_id)
{
    if (!CcSimArchiveStaffMember(sim, person_id)) return false;
    const CcCharacter *person = CcSimCharacter(sim, person_id);
    const CcSettlement *seat = CcArchiveSeat(sim);
    return person != NULL && person->death_day > sim->current_day &&
        person->occupation == CC_OCCUPATION_SCRIBE && person->activity == CC_CHARACTER_ACTIVITY_WORKING &&
        person->bandit_group_id == 0 && seat != NULL && seat->id == sim->archive_staff.seat_id &&
        !CcSettlementIsAbandoned(seat) && person->current_settlement_id == seat->id;
}
int32_t CcSimArchiveStaffSlots(const CcSim *sim)
{
    if (sim == NULL) return 0;
    if (sim->schema_version < 85U || !sim->archive_staff.active) return sim->archives.scribes;
    int32_t count = sim->archive_staff.legacy_scribes;
    for (int32_t i = 0; i < CC_MAX_SCRIBES; ++i) count += sim->archive_staff.person_ids[i] != 0;
    return count;
}
int32_t CcSimArchiveStaffCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    if (sim->schema_version < 85U || !sim->archive_staff.active) return sim->archives.scribes;
    const CcSettlement *seat = CcArchiveSeat(sim);
    if (seat == NULL || seat->id != sim->archive_staff.seat_id) return 0;
    int32_t count = sim->archive_staff.legacy_scribes;
    for (int32_t i = 0; i < CC_MAX_SCRIBES; ++i)
        count += CcSimArchiveStaffWorking(sim, sim->archive_staff.person_ids[i]);
    return count;
}
void CcSimRefreshArchiveStaff(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 85U || !sim->archive_staff.active) return;
    for (int32_t i = 0; i < CC_MAX_SCRIBES; ++i) {
        const CcCharacter *person = CcSimCharacter(sim, sim->archive_staff.person_ids[i]);
        if (person == NULL || person->death_day <= sim->current_day) sim->archive_staff.person_ids[i] = 0;
    }
    sim->archives.scribes = CcSimArchiveStaffCount(sim);
}
bool CcSimArchiveStaffValid(const CcSim *sim)
{
    if (sim == NULL) return false;
    if (sim->schema_version < 85U) return true;
    const CcArchiveStaff *staff = &sim->archive_staff;
    if (staff->legacy_scribes < 0 || staff->legacy_scribes > CC_MAX_SCRIBES) return false;
    if (staff->active ? CcSimSettlement(sim, staff->seat_id) == NULL :
        staff->seat_id != 0 || staff->legacy_scribes != 0) return false;
    for (int32_t i = 0; i < CC_MAX_SCRIBES; ++i) {
        CcId id = staff->person_ids[i];
        if (id == 0) continue;
        if (!staff->active || CcIdKind(id) != CC_ENTITY_CHARACTER ||
            (id & CC_ID_SERIAL_MASK) == 0 || (id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial) return false;
        for (int32_t j = 0; j < i; ++j) if (staff->person_ids[j] == id) return false;
    }
    return !staff->active || CcSimArchiveStaffSlots(sim) <= CC_MAX_SCRIBES;
}
