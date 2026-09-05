#include "metagame/cc_metagame.h"
#include "persistence/cc_save.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static CcSim sim;
static CcSim restored;
static char error[256];

static void CheckValid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "Gossip fixture: %s\n", error);
        CC_CHECK(false);
    }
}

static void Prepare(void)
{
    CcSimInit(&sim, 42U);
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i) {
        sim.royal_carriages[i].next_dispatch_day = sim.current_day + 7;
        sim.royal_carriages[i].condition = 0;
    }
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 10000;
        sim.settlements[i].stock[CC_GOOD_PAPER] = 20;
        sim.settlements[i].stock[CC_GOOD_TOOLS] = 20;
        sim.settlements[i].stock[CC_GOOD_GOLD] = 20;
        sim.settlements[i].stock[CC_GOOD_GEMS] = 20;
    }
    for (int32_t i = 0; i < sim.route_count; ++i) {
        sim.routes[i].closed = false;
        sim.routes[i].condition = 100;
        sim.routes[i].security = 100;
    }
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
}

static CcId AddAccount(CcId origin, const char *text)
{
    CC_CHECK(sim.event_count < CC_MAX_EVENTS);
    CcEvent *event = &sim.events[sim.event_write_index];
    *event = (CcEvent){
        .id = CcMakeId(CC_ENTITY_EVENT, sim.next_entity_serial++),
        .day = sim.current_day,
        .kind = CC_EVENT_KINGDOM_ACTION,
        .subject_id = sim.kingdoms[0].id,
        .location_id = origin,
        .magnitude = 40
    };
    (void)snprintf(event->text, sizeof(event->text), "%s", text);
    sim.event_write_index = (sim.event_write_index + 1) % CC_MAX_EVENTS;
    sim.event_count += 1;
    return event->id;
}

static CcGossip *Account(CcId id)
{
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        if (sim.gossip[i].event_id == id) return &sim.gossip[i];
    }
    CC_CHECK(false);
    return NULL;
}

static void Depart(CcId destination)
{
    CcCommand command = {.kind = CC_COMMAND_TRAVEL, .target_id = destination};
    CC_CHECK(CcSimApply(&sim, &command, error, sizeof(error)));
    sim.journey.ambush_pending = false;
}

static void Arrive(void)
{
    for (int32_t step = 0; step < 10000 && sim.journey.active; ++step) {
        if (sim.journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(&sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            CC_CHECK(CcSimApply(&sim, &rest, error, sizeof(error)));
        } else {
            CC_CHECK(sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
            CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_TICKS_PER_SECOND);
        }
    }
    CC_CHECK(!sim.journey.active);
}

static void CheckLocalAndRemoteAccounts(void)
{
    Prepare();
    CcId remote = AddAccount(sim.settlements[0].id, "The western court pledges a new bridge.");
    CcId local = AddAccount(sim.settlements[1].id, "The abbot blesses the town well.");
    CcSimAdvanceDays(&sim, 6);
    CC_CHECK(Account(remote)->heard_day == 0);
    CC_CHECK(!Account(remote)->recorded);
    CC_CHECK(Account(local)->heard_day == 2);
    CC_CHECK(Account(local)->recorded);
    CC_CHECK(Account(remote)->settlement_mask == 1U);
}

static void CheckArrivalAndLateRecording(void)
{
    Prepare();
    CcId remote = AddAccount(sim.settlements[0].id, "The western court pledges a new bridge.");
    Depart(sim.settlements[1].id);
    CC_CHECK(Account(remote)->heard_day == 0);
    CcSimAdvanceRuntimeTicks(&sim, 1);
    CC_CHECK(Account(remote)->settlement_mask == 1U);
    const char *path = "gossip-journey.ccsave";
    CheckValid();
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    (void)remove(path);
    sim = restored;
    Arrive();
    CC_CHECK(Account(remote)->heard_day == sim.current_day);
    CC_CHECK(strstr(Account(remote)->heard_from, "travelers") != NULL);
    CcId heard = Account(remote)->heard_event_id;
    const CcEvent *receipt = CcSimEvent(&sim, heard);
    CC_CHECK(receipt != NULL && receipt->parent_id == remote);
    CC_CHECK(receipt->location_id == sim.settlements[1].id);
    sim.settlements[1].stock[CC_GOOD_PAPER] = 0;
    sim.settlements[1].production[CC_GOOD_PAPER] = 0;
    sim.settlements[1].service_mask &= ~(UINT32_C(1) << CC_SERVICE_MILL);
    CcSimAdvanceDays(&sim, 14);
    CC_CHECK(!Account(remote)->recorded);
    sim.settlements[1].stock[CC_GOOD_PAPER] = 20;
    sim.iron_ledger_reserve = 50;
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(Account(remote)->recorded);
    int32_t records = 0;
    for (int32_t i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event->kind == CC_EVENT_LORE_RECORDED && event->parent_id == heard) {
            CC_CHECK(event->location_id == sim.settlements[1].id);
            records += 1;
        }
    }
    CC_CHECK(records == 1);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckCourierRelay(void)
{
    Prepare();
    CcId report = AddAccount(sim.settlements[2].id, "A new guild opens in the eastern town.");
    sim.courier_count = 1;
    sim.couriers[0] = (CcCourier){
        .id = CcMakeId(CC_ENTITY_COURIER, sim.next_entity_serial++),
        .kind = CC_COURIER_PEACE_OFFER, .status = CC_COURIER_WAITING,
        .issuer_kingdom_id = sim.settlements[2].kingdom_id,
        .recipient_kingdom_id = sim.settlements[1].kingdom_id,
        .origin_settlement_id = sim.settlements[2].id,
        .current_settlement_id = sim.settlements[2].id,
        .destination_settlement_id = sim.settlements[1].id,
        .departure_day = 2, .reliability = 100
    };
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.couriers[0].status == CC_COURIER_TRAVELLING);
    CC_CHECK(Account(report)->heard_day == 0);
    int32_t arrival = sim.couriers[0].arrival_day;
    CcSimAdvanceDays(&sim, arrival - sim.current_day);
    CC_CHECK(Account(report)->heard_day == arrival);
    CC_CHECK(strcmp(Account(report)->heard_from, "Royal couriers") == 0);
}

static void CheckLostCourier(void)
{
    Prepare();
    CcId report = AddAccount(sim.settlements[2].id, "The eastern road has a new shrine.");
    sim.courier_count = 1;
    sim.couriers[0] = (CcCourier){
        .id = CcMakeId(CC_ENTITY_COURIER, sim.next_entity_serial++),
        .kind = CC_COURIER_PEACE_OFFER, .status = CC_COURIER_WAITING,
        .current_settlement_id = sim.settlements[2].id,
        .destination_settlement_id = sim.settlements[1].id,
        .departure_day = 2, .reliability = 100
    };
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.couriers[0].status == CC_COURIER_TRAVELLING);
    sim.couriers[0].status = CC_COURIER_LOST;
    CcSimAdvanceDays(&sim, 6);
    CC_CHECK(Account(report)->heard_day == 0);
    CC_CHECK(!Account(report)->recorded);
}

static void CheckJournalAndLegacyReplay(void)
{
    const char *path = "gossip-journal.ccsave";
    Prepare();
    CcId report = AddAccount(sim.settlements[1].id, "The local guild signs a charter.");
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 6, error, sizeof(error)));
    CC_CHECK(Account(report)->recorded);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);

    Prepare();
    sim.schema_version = 41U;
    AddAccount(sim.settlements[0].id, "An account from the older archive.");
    CcSimAdvanceDays(&sim, 6);
    CC_CHECK(sim.archives.lore_stored > 0);
    CC_CHECK(sim.gossip_last_event_id == 0U);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.gossip_last_event_id == 0U);
    CC_CHECK(restored.archives.lore_stored == sim.archives.lore_stored);
    (void)remove(path);
}

int main(void)
{
    CheckLocalAndRemoteAccounts();
    CheckArrivalAndLateRecording();
    CheckCourierRelay();
    CheckLostCourier();
    CheckJournalAndLegacyReplay();
    puts("Traveler gossip network passed.");
    return 0;
}
