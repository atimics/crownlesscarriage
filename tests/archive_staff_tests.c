#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_staff.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
static CcSim sim, before, restored, roster;
static char error[256];
static CcId recruit, seat_id, volume_id;
static CcCharacter *Person(void)
{
    for (int i = 0; i < sim.character_count; ++i) if (sim.characters[i].id == recruit) return &sim.characters[i];
    return NULL;
}
static CcTreasure *Volume(void)
{
    for (int i = 0; i < sim.treasure_count; ++i) if (sim.treasures[i].id == volume_id) return &sim.treasures[i];
    return NULL;
}
static void RoundTrip(void)
{
    static int check = 0; ++check;
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "Round trip %d: %s\n", check, error); CC_CHECK(false); }
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(memcmp(sim.archive_staff.person_ids, restored.archive_staff.person_ids, sizeof(sim.archive_staff.person_ids)) == 0);
    CC_CHECK(sim.archive_staff.active == restored.archive_staff.active);
    CC_CHECK(sim.archive_staff.seat_id == restored.archive_staff.seat_id);
    CC_CHECK(sim.archive_staff.legacy_scribes == restored.archive_staff.legacy_scribes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}
static void Prepared(void)
{
    CcSimInit(&sim, 42U);
    CcArchiveRecruitmentPlan p = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY);
    recruit = p.person_id; seat_id = p.seat_id;
    for (int i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].id != recruit) sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    Person()->current_settlement_id = seat_id; Person()->death_day = 30000;
    sim.archives.scribes = 0;
    CC_CHECK(CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_ARRIVED);
    for (int day = 0; day < 7; ++day) {
        sim.current_day += 1; sim.royal_trade_week = sim.current_day / 7;
        CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) ==
            (day == 6 ? CC_ARCHIVE_TRAINING_COMPLETE : CC_ARCHIVE_TRAINING_WORKED));
    }
    CC_CHECK(CcSimArchiveAppointmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_BUSY);
    CC_CHECK(sim.treasure_count < CC_MAX_TREASURES);
    CcTreasure *volume = &sim.treasures[sim.treasure_count++];
    *volume = (CcTreasure){.id = CcMakeId(CC_ENTITY_TREASURE, sim.next_entity_serial++),
        .owner_id = seat_id, .location_id = seat_id, .maker_settlement_id = seat_id,
        .gold_content = 1, .gem_content = 1, .craft_work = 1, .appraised_value = 6, .created_day = sim.current_day};
    (void)snprintf(volume->name, sizeof(volume->name), "Chronicle of the local archive");
    volume_id = volume->id;
    CcSimUpgradeArchivePhysicalLore(&sim);
    sim.current_day += 1; sim.royal_trade_week = sim.current_day / 7;
    RoundTrip();
}
static CcId Account(CcId town)
{
    CC_CHECK(sim.event_count < CC_MAX_EVENTS);
    CcEvent *event = &sim.events[sim.event_write_index];
    *event = (CcEvent){.id = CcMakeId(CC_ENTITY_EVENT, sim.next_entity_serial++), .day = sim.current_day,
        .kind = CC_EVENT_KINGDOM_ACTION, .subject_id = sim.kingdoms[0].id, .location_id = town, .magnitude = 40};
    (void)snprintf(event->text, sizeof(event->text), "A local witness describes a disputed royal levy.");
    CcId id = event->id;
    sim.event_write_index = (sim.event_write_index + 1) % CC_MAX_EVENTS; sim.event_count += 1;
    CcSimRefreshCharacterGossip(&sim);
    return id;
}
int main(void)
{
    Prepared();
    CcArchiveAppointmentPlan p = CcSimArchiveAppointmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.volume_id == volume_id);
    before = sim;
    CcSettlement *seat = CcSimSettlementMutable(&sim, seat_id);
    seat->stock[CC_GOOD_TOOLS] = CC_SIM_MAX_UNITS;
    CC_CHECK(CcSimArchiveAppointmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_STORAGE);
    CcArchiveStaff staff = sim.archive_staff;
    CC_CHECK(!CcSimAppointArchiveRecruit(&sim));
    CC_CHECK(memcmp(&staff, &sim.archive_staff, sizeof(staff)) == 0);
    sim = before; sim.archives.scribes = CC_MAX_SCRIBES;
    CC_CHECK(CcSimArchiveAppointmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_FULL);
    sim = before;
    int lore = sim.archives.lore_stored;
    int paper = seat->stock[CC_GOOD_PAPER], wheat = seat->stock[CC_GOOD_WHEAT], tools = seat->stock[CC_GOOD_TOOLS];
    sim.archives.kit_tool_wear = 7;
    before = sim;
    CC_CHECK(CcSimAppointArchiveRecruit(&sim));
    CC_CHECK(CcSimArchiveStaffMember(&sim, recruit) && CcSimArchiveStaffCount(&sim) == 1);
    CC_CHECK(sim.archives.scribes == 1 && sim.archive_recruitment.status == 0);
    CC_CHECK(Volume()->craft_work == 2 && Volume()->location_id == seat_id && Volume()->owner_id == seat_id);
    CC_CHECK(sim.archives.lore_stored == lore + 1);
    CC_CHECK(seat->stock[CC_GOOD_PAPER] == paper && seat->stock[CC_GOOD_WHEAT] == wheat);
    CC_CHECK(seat->stock[CC_GOOD_TOOLS] == tools && sim.archives.kit_tool_wear == 0);
    CC_CHECK(memcmp(sim.gossip_carriers, before.gossip_carriers, sizeof(sim.gossip_carriers)) == 0);
    const CcEvent *event = CcSimRecentEvent(&sim, 0);
    CC_CHECK(event->actor_id == recruit && event->subject_id == volume_id && event->location_id == seat_id);
    RoundTrip();
    before = sim;
    CC_CHECK(!CcSimAppointArchiveRecruit(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    roster = sim;
    int slot = 1;
    for (int i = 0; i < sim.character_count && slot < CC_MAX_SCRIBES; ++i)
        if (sim.characters[i].id != recruit) sim.archive_staff.person_ids[slot++] = sim.characters[i].id;
    CC_CHECK(slot == CC_MAX_SCRIBES);
    RoundTrip(); before = sim;
    for (int i = 0; i < CC_MAX_SCRIBES; ++i) {
        sim = before; sim.archive_staff.person_ids[i] = 0;
        CC_CHECK(CcSimHash(&sim) != CcSimHash(&before)); RoundTrip();
    }
    sim = before; sim.archive_staff.seat_id = sim.settlements[0].id;
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before)); RoundTrip();
    sim = before; sim.archive_staff.active = false;
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before));
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.archive_staff.person_ids[3] = 0; before = sim;
    sim.archive_staff.legacy_scribes = 1;
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before)); RoundTrip();
    sim = roster;
    Person()->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
    CC_CHECK(CcSimArchiveStaffCount(&sim) == 0);
    Person()->activity = CC_CHARACTER_ACTIVITY_WORKING;
    sim.iron_ledger_reserve = 0;
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(CcSimArchiveStaffCount(&sim) == 1 && sim.archives.scribes == 1);
    Person()->death_day = sim.current_day + 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(!CcSimArchiveStaffMember(&sim, recruit) && sim.archives.scribes == 0);
    RoundTrip();
    Prepared();
    Volume()->owner_id = Volume()->location_id = sim.settlements[0].id;
    CC_CHECK(Volume()->location_id != seat_id);
    for (int i = 0; i < CcSimGossipCarrierCapacity(&sim); ++i)
        if (sim.gossip_carriers[i].id == recruit) sim.gossip_carriers[i].stories = 0;
    CC_CHECK(CcSimArchiveAppointmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_RECORDS);
    before = sim;
    CC_CHECK(!CcSimAppointArchiveRecruit(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    (void)Account(seat_id);
    p = CcSimArchiveAppointmentPlan(&sim);
    CC_CHECK(p.source_event_id != 0 && p.account_slot >= 0 && p.gate == CC_ARCHIVE_RECRUIT_READY);
    seat = CcSimSettlementMutable(&sim, seat_id);
    seat->stock[CC_GOOD_GOLD] = seat->stock[CC_GOOD_GEMS] = 1;
    p = CcSimArchiveAppointmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.volume_id == 0);
    CcId source = p.source_event_id;
    const CcGossipCarrier *held = CcSimGossipCarrier(&sim, recruit);
    CC_CHECK(held != NULL && (held->stories & (UINT32_C(1) << (uint32_t)p.account_slot)) != 0);
    CcMoney coins = CcSimTrackedGold(&sim);
    int raw_gold = CcSimTrackedGood(&sim, CC_GOOD_GOLD), raw_gems = CcSimTrackedGood(&sim, CC_GOOD_GEMS);
    int raw_paper = CcSimTrackedGood(&sim, CC_GOOD_PAPER), raw_wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    CC_CHECK(CcSimAppointArchiveRecruit(&sim));
    CC_CHECK(CcSimTrackedGold(&sim) == coins);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == raw_gold && CcSimTrackedGood(&sim, CC_GOOD_GEMS) == raw_gems);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_PAPER) == raw_paper - 1 && CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == raw_wheat - 2);
    event = CcSimRecentEvent(&sim, 0);
    CC_CHECK(event->actor_id == recruit && event->parent_id == source);
    CC_CHECK(event->subject_id != volume_id);
    volume_id = event->subject_id;
    CC_CHECK(Volume()->location_id == seat_id && Volume()->owner_id == seat_id && Volume()->craft_work == 1);
    CC_CHECK(seat->stock[CC_GOOD_GOLD] == 1 && seat->stock[CC_GOOD_GEMS] == 1);
    CC_CHECK(Volume()->gold_content == 0 && Volume()->gem_content == 0 && Volume()->appraised_value == 1);
    RoundTrip();
    before = sim;
    Volume()->destroyed = true;
    CcSimUpgradeArchivePhysicalLore(&sim);
    CC_CHECK(sim.archives.lore_stored == before.archives.lore_stored - 1);
    RoundTrip();
    sim = before;
    char old_name[CC_MAP_NAME_CAPACITY];
    (void)snprintf(old_name, sizeof(old_name), "%s", Volume()->name);
    (void)snprintf(Volume()->name, sizeof(Volume()->name), "Ruined %.40s", old_name);
    CcSimUpgradeArchivePhysicalLore(&sim);
    CC_CHECK(sim.archives.lore_stored == before.archives.lore_stored - 1);
    RoundTrip();
    sim = before; Volume()->gold_content = 1;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; (void)snprintf(Volume()->name, sizeof(Volume()->name), "A jeweled crown");
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.schema_version = 83U;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before;
    Prepared();
    CC_CHECK(CcSimAppointArchiveRecruit(&sim));
    sim.current_day = 13; sim.royal_trade_week = 1;
    for (int i = 0; i < sim.event_count; ++i) sim.events[i].magnitude = 0;
    memset(sim.gossip, 0, sizeof(sim.gossip));
    memset(sim.gossip_carriers, 0, sizeof(sim.gossip_carriers));
    for (int i = 0; i < sim.royal_carriage_count; ++i) sim.royal_carriages[i].condition = 0;
    seat = CcSimSettlementMutable(&sim, seat_id);
    seat->stock[CC_GOOD_GOLD] = seat->stock[CC_GOOD_GEMS] = 0;
    seat->stock[CC_GOOD_WHEAT] = 10000;
    seat->stock[CC_GOOD_PAPER] = seat->stock[CC_GOOD_TOOLS] = 10;
    CcId distant = Account(sim.settlements[0].id);
    int account_slot = -1;
    for (int i = 0; i < CC_MAX_GOSSIP; ++i)
        if (sim.gossip[i].event_id == distant) account_slot = i;
    CC_CHECK(account_slot >= 0);
    sim.gossip[account_slot].heard_day = sim.current_day;
    sim.gossip[account_slot].heard_event_id = distant;
    sim.gossip[account_slot].heard = sim.gossip[account_slot].local[0];
    (void)snprintf(sim.gossip[account_slot].heard_from, CC_NAME_CAPACITY, "A distant messenger");
    before = sim;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.gossip[account_slot].event_id == distant && !sim.gossip[account_slot].recorded);
    sim = before;
    bool gave_account = false;
    for (int i = 0; i < CcSimGossipCarrierCapacity(&sim); ++i)
        if (sim.gossip_carriers[i].id == recruit) {
            sim.gossip_carriers[i].stories |= UINT32_C(1) << (uint32_t)account_slot;
            sim.gossip_carriers[i].versions[account_slot] = sim.gossip[account_slot].heard;
            gave_account = true;
        }
    CC_CHECK(gave_account);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.gossip[account_slot].event_id == distant && sim.gossip[account_slot].recorded);
    RoundTrip();
    Prepared();
    const char *path = "archive-staff-journal.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    CC_CHECK(CcSimArchiveStaffMember(&sim, recruit));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "UPDATE archive_staff SET active=4294967297;", NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(strstr(error, "integer") != NULL);
    (void)remove(path);
    puts("Named appointments preserve local records, resources, identity and journal replay.");
    return 0;
}
