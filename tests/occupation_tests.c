#include "sim/cc_occupations.h"
#include "persistence/cc_save.h"
#include "test_support.h"

#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

static CcSim sim, saved, before;
static char error[256];

static CcId AddEvent(CcEventKind kind, CcId place)
{
    CcEvent *event = &sim.events[sim.event_write_index];
    *event = (CcEvent){.id = CcMakeId(CC_ENTITY_EVENT, sim.next_entity_serial++),
        .day = sim.current_day, .kind = kind, .location_id = place, .magnitude = 1};
    (void)snprintf(event->text, sizeof(event->text), "A calf is born at Thornford.");
    sim.event_write_index = (sim.event_write_index + 1) % CC_MAX_EVENTS;
    if (sim.event_count < CC_MAX_EVENTS) ++sim.event_count;
    return event->id;
}

static int32_t Slot(CcId event)
{
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i)
        if (sim.gossip[i].event_id == event) return i;
    CC_CHECK(false);
    return 0;
}

static bool Holds(CcId person, int32_t slot)
{
    const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, person);
    return carrier != NULL && (carrier->stories & (UINT32_C(1) << (uint32_t)slot)) != 0;
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "Occupation fixture: %s\n", error);
        CC_CHECK(false);
    }
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &saved, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&saved));
    CC_CHECK(memcmp(sim.characters, saved.characters, sizeof(sim.characters)) == 0);
    CC_CHECK(memcmp(sim.gossip_carriers, saved.gossip_carriers, sizeof(sim.gossip_carriers)) == 0);
}

int main(void)
{
    CcSimInit(&sim, 42U);
    int32_t town = -1;
    for (int32_t i = 0; i < sim.settlement_count; ++i)
        if (strcmp(sim.settlements[i].name, "Thornford") == 0) town = i;
    CC_CHECK(town >= 0);
    int shepherds = 0;
    for (int32_t i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].home_settlement_id == sim.settlements[town].id &&
            sim.characters[i].occupation == CC_OCCUPATION_SHEPHERD) ++shepherds;
    CC_CHECK(shepherds > 0);
    before = sim;
    CcCharacter swap = sim.characters[0];
    sim.characters[0] = sim.characters[1]; sim.characters[1] = swap;
    for (int32_t i = 0; i < sim.character_count; ++i)
        CC_CHECK(CcSimInitialOccupation(&sim, sim.characters[i].home_settlement_id,
            sim.characters[i].id) == sim.characters[i].occupation);
    sim = before;
    CcCharacter *shepherd = &sim.characters[0], *smith = &sim.characters[1];
    shepherd->current_settlement_id = smith->current_settlement_id = sim.settlements[town].id;
    shepherd->occupation = CC_OCCUPATION_SHEPHERD;
    shepherd->role = CC_CHARACTER_REFUGEE;
    smith->occupation = CC_OCCUPATION_SMITH;
    smith->role = CC_CHARACTER_LABORER;
    shepherd->activity = smith->activity = CC_CHARACTER_ACTIVITY_WORKING;
    shepherd->birth_day = smith->birth_day = sim.current_day - 30 * 365;
    shepherd->death_day = smith->death_day = sim.current_day + 30 * 365;
    CcId event = AddEvent(CC_EVENT_COW_CALVING, sim.settlements[town].id);
    CcSimRefreshCharacterGossip(&sim);
    int32_t slot = Slot(event);
    CC_CHECK(Holds(shepherd->id, slot));
    CC_CHECK(!Holds(smith->id, slot));
    const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, shepherd->id);
    CC_CHECK(carrier->versions[slot].source_character_id == shepherd->id);
    CC_CHECK(carrier->versions[slot].retellings == 0);
    CC_CHECK(carrier->versions[slot].confidence == 100);
    /* The shepherd carries the account to the smith in another town. */
    int32_t destination = (town + 1) % sim.settlement_count;
    shepherd->current_settlement_id = smith->current_settlement_id =
        sim.settlements[destination].id;
    CcSimRefreshCharacterGossip(&sim);
    CC_CHECK(Holds(smith->id, slot));
    carrier = CcSimGossipCarrier(&sim, smith->id);
    CcGossipVersion received = carrier->versions[slot];
    CC_CHECK(received.source_character_id == shepherd->id);
    CC_CHECK(received.retellings == 2);
    smith->occupation = CC_OCCUPATION_NONE;
    CcSimRefreshCharacterGossip(&sim);
    CC_CHECK(memcmp(&received, &carrier->versions[slot], sizeof(received)) == 0);
    CC_CHECK(shepherd->role == CC_CHARACTER_REFUGEE);
    RoundTrip();
    uint64_t hash = CcSimHash(&sim);
    shepherd->role = CC_CHARACTER_LABORER;
    CC_CHECK(shepherd->occupation == CC_OCCUPATION_SHEPHERD);
    CC_CHECK(CcSimHash(&sim) != hash);
    RoundTrip();
    before = sim;
    shepherd->occupation = (CcCharacterOccupation)-1;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    shepherd->occupation = CC_OCCUPATION_COUNT;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before;
    shepherd->current_settlement_id = sim.settlements[town].id;
    CcSimAdvanceDays(&sim, 3);
    /* A recent local event can be observed; an older event needs an account. */
    event = AddEvent(CC_EVENT_SHEEP_SHEARED, sim.settlements[town].id);
    sim.events[(sim.event_write_index + CC_MAX_EVENTS - 1) % CC_MAX_EVENTS].day -= 3;
    CcSimRefreshCharacterGossip(&sim);
    CC_CHECK(!Holds(shepherd->id, Slot(event)));
    /* A received account keeps its source after that lifetime ends. */
    CcId source_id = shepherd->id;
    shepherd->death_day = sim.current_day + 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.characters[0].id != source_id);
    CC_CHECK(sim.characters[0].occupation == CC_OCCUPATION_SHEPHERD);
    carrier = CcSimGossipCarrier(&sim, smith->id);
    CC_CHECK(carrier != NULL && Holds(smith->id, slot));
    CC_CHECK(carrier->versions[slot].source_character_id == source_id);
    RoundTrip();
    /* Read the full SQLite integer before applying the enum bounds. */
    const char *path = "occupation-corrupt.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database,
        "UPDATE npc_character SET occupation=4294967296 WHERE slot=0;",
        NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &saved, error, sizeof(error)));
    CC_CHECK(strstr(error, "occupation") != NULL);
    (void)remove(path);
    puts("Occupation identity, local observations and received accounts passed.");
    return 0;
}
