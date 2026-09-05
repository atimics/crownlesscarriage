#include "sim/cc_sim.h"

#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static void CheckReferences(const CcSim *sim)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        CC_CHECK(event != NULL);
        CC_CHECK(event->parent_id == 0U ||
                 CcSimEvent(sim, event->parent_id) != NULL);
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        CC_CHECK(situation->cause_event_id == 0U ||
                 CcSimEvent(sim, situation->cause_event_id) != NULL);
    }
}

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

/* The named cast must turn over on its own: the initial age pyramid spans
 * working age to elders, so a long chronicle sees deaths, heirs, and
 * generational change even without a player. The event ring only holds the
 * last 256 entries, so the scan runs in monthly chunks: any birth or death
 * event stays in the window far longer than a chunk. */
static void CheckPopulationTurnover(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(2));
    bool birth_event = false, death_event = false;
    int32_t last_seen_day = 0;
    for (int32_t month = 0; month < 25 * 12; ++month) {
        CcSimAdvanceDays(&sim, 30);
        for (int32_t i = 0; i < sim.event_count; ++i) {
            const CcEvent *event = CcSimRecentEvent(&sim, i);
            if (event == NULL || event->day <= last_seen_day) continue;
            if (event->kind == CC_EVENT_CHARACTER_BORN) birth_event = true;
            if (event->kind == CC_EVENT_CHARACTER_DIED) death_event = true;
        }
        const CcEvent *newest = CcSimRecentEvent(&sim, 0);
        if (newest != NULL && newest->day > last_seen_day) {
            last_seen_day = newest->day;
        }
    }
    CC_CHECK(sim.character_deaths > 0);
    CC_CHECK(sim.character_births == sim.character_deaths);
    CC_CHECK(sim.character_count == CC_MAX_CHARACTERS);
    bool heir_exists = false;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].generation > 0) heir_exists = true;
    }
    CC_CHECK(heir_exists);
    CC_CHECK(death_event);
    CC_CHECK(birth_event);
    CheckReferences(&sim);
}

/* A ruler's heir inherits the throne but not the anointment: the abbey must
 * sanction the succession before the crown becomes lawful again. */
static void CheckRulerSuccessionClearsAnointment(char *error, size_t capacity)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x60b121));
    CcSimAdvanceDays(&sim, 365);
    int32_t slot = -1;
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        if (sim.kingdoms[i].anointed) { slot = i; break; }
    }
    CC_CHECK(slot >= 0);
    CcId ruler_id = sim.kingdoms[slot].ruler_character_id;
    CcCharacter *ruler = (CcCharacter *)CcSimCharacter(&sim, ruler_id);
    CC_CHECK(ruler != NULL);
    int32_t ruler_generation = ruler->generation;
    ruler->death_day = sim.current_day;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimValidate(&sim, error, capacity));
    CC_CHECK(!sim.kingdoms[slot].anointed);
    CC_CHECK(sim.kingdoms[slot].anointed_by_character_id == 0U);
    CC_CHECK(sim.kingdoms[slot].ruler_character_id != ruler_id);
    const CcCharacter *heir = CcSimCharacter(
        &sim, sim.kingdoms[slot].ruler_character_id);
    CC_CHECK(heir != NULL);
    CC_CHECK(heir->ancestor_id == ruler_id);
    CC_CHECK(heir->generation == ruler_generation + 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_ROYAL_SUCCESSION) >= 1);
    CheckReferences(&sim);
}

/* The abbot's death hands the anointing roll to the successor; kingdoms
 * anointed by the old abbot follow the new one. */
static void CheckAbbotSuccessionMovesAnointment(char *error, size_t capacity)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x60b121));
    CcSimAdvanceDays(&sim, 365);
    CcCharacter *abbot = (CcCharacter *)CcSimCharacter(
        &sim, sim.archives.abbot_character_id);
    CC_CHECK(abbot != NULL);
    CcId old_abbot = sim.archives.abbot_character_id;
    abbot->death_day = sim.current_day;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimValidate(&sim, error, capacity));
    CC_CHECK(sim.archives.abbot_character_id != old_abbot);
    CC_CHECK(CountEvents(&sim, CC_EVENT_MONASTIC_SUCCESSION) >= 1);
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        if (!sim.kingdoms[i].anointed) continue;
        CC_CHECK(sim.kingdoms[i].anointed_by_character_id ==
                 sim.archives.abbot_character_id);
    }
    CheckReferences(&sim);
}

int main(void)
{
    char error[192];
    CheckPopulationTurnover();
    CheckRulerSuccessionClearsAnointment(error, sizeof(error));
    CheckAbbotSuccessionMovesAnointment(error, sizeof(error));
    for (uint32_t seed = 1U; seed <= 8U; ++seed) {
        CcSim sim;
        CcSimInit(&sim, seed * UINT32_C(0x9e3779b9));
        CcSimAdvanceDays(&sim, 3650);
        CC_CHECK(sim.event_count == CC_MAX_EVENTS);
        CheckReferences(&sim);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    }

    CcSim broken;
    CcSimInit(&broken, UINT32_C(0xca05a1));
    CcEvent *newest = (CcEvent *)CcSimRecentEvent(&broken, 0);
    CC_CHECK(newest != NULL);
    newest->parent_id = CcMakeId(CC_ENTITY_EVENT, UINT64_C(0xffffff));
    CC_CHECK(!CcSimValidate(&broken, error, sizeof(error)));
    CC_CHECK(strstr(error, "lost parent") != NULL);

    const char *save_path = "/tmp/crownless-causal-history-tests.ccsave";
    (void)remove(save_path);
    CcSim saved;
    CcSim loaded;
    CcSimInit(&saved, UINT32_C(42));
    CcSimAdvanceDays(&saved, 3650);
    CC_CHECK(CcSaveWrite(save_path, &saved, error, sizeof(error)));
    CC_CHECK(CcSaveRead(save_path, &loaded, error, sizeof(error)));
    CheckReferences(&loaded);
    CC_CHECK(CcSimHash(&saved) == CcSimHash(&loaded));
    CC_CHECK(remove(save_path) == 0);

    puts("Causal history retention tests passed");
    return 0;
}
