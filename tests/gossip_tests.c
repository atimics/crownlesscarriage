#include "metagame/cc_metagame.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include "story/cc_speech.h"

#include <stdio.h>
#include <sqlite3.h>
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
    /* Each fixture supplies its own notable accounts. */
    for (int32_t i = 0; i < sim.event_count; ++i) sim.events[i].magnitude = 0;
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
    CC_CHECK(receipt != NULL);
    const CcEvent *shared = CcSimEvent(&sim, receipt->parent_id);
    CC_CHECK(shared != NULL && shared->parent_id == remote);
    CC_CHECK(shared->subject_id == sim.player.id);
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

static void CheckRelayAndBlockedRoad(void)
{
    Prepare();
    CcId report = AddAccount(sim.settlements[3].id, "The mine town opens a new market.");
    CcRoyalCarriage *carriage = &sim.royal_carriages[0];
    carriage->location_id = sim.settlements[2].id;
    carriage->route_id = sim.routes[2].id;
    carriage->destination_id = sim.settlements[3].id;
    carriage->target_id = sim.settlements[1].id;
    carriage->mode = CC_ROYAL_CARRIAGE_REPOSITIONING;
    carriage->departure_day = sim.current_day;
    carriage->arrival_day = sim.current_day + 1;
    carriage->condition = 100;
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        for (int32_t j = i + 1; j < sim.kingdom_count; ++j) {
            sim.diplomacy[i][j] = sim.diplomacy[j][i] = CC_DIPLOMACY_ALLIANCE;
        }
    }
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(carriage->location_id == sim.settlements[3].id);
    CC_CHECK(Account(report)->heard_day == 0);
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        for (int32_t j = i + 1; j < sim.kingdom_count; ++j) {
            sim.diplomacy[i][j] = sim.diplomacy[j][i] = CC_DIPLOMACY_WAR;
        }
    }
    CcSimAdvanceDays(&sim, carriage->arrival_day - sim.current_day);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(Account(report)->heard_day == 0);
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        for (int32_t j = i + 1; j < sim.kingdom_count; ++j) {
            sim.diplomacy[i][j] = sim.diplomacy[j][i] = CC_DIPLOMACY_ALLIANCE;
        }
    }
    for (int32_t day = 0; day < 12 && Account(report)->heard_day == 0; ++day) {
        CcSimAdvanceDays(&sim, 1);
    }
    CC_CHECK(Account(report)->heard_day > 0);
    CC_CHECK(strcmp(Account(report)->heard_from, "Carriage travelers") == 0);
    CC_CHECK((Account(report)->settlement_mask & (UINT32_C(1) << 2U)) != 0U);
}

static void CheckStorySlotReuse(void)
{
    Prepare();
    sim.archives.scribes = 0;
    sim.iron_ledger_reserve = 0;
    CcId first = AddAccount(sim.settlements[0].id, "The western town lights a beacon.");
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(Account(first)->heard_day == 0);
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        AddAccount(sim.settlements[2].id, "A fresh eastern account.");
    }
    CcSimAdvanceDays(&sim, 1);
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        CC_CHECK(sim.gossip[i].event_id != first);
    }
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        if (sim.gossip_carriers[i].id != sim.player.id) continue;
        /* The flood never reaches the player; their own town's notices may. */
        for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
            if ((sim.gossip_carriers[i].stories & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
            CC_CHECK(strcmp(sim.gossip[slot].text, "A fresh eastern account.") != 0);
        }
    }
    Depart(sim.settlements[1].id);
    Arrive();
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        /* No flooded account reaches the next town; carried notices may. */
        CC_CHECK(strcmp(sim.gossip[i].text, "A fresh eastern account.") != 0 ||
                 (sim.gossip[i].settlement_mask & 2U) == 0U);
    }
    CheckValid();
}

static void CheckHearingOrder(void)
{
    Prepare();
    CcId first = AddAccount(sim.settlements[1].id, "The oldest local account.");
    CcId second = AddAccount(sim.settlements[1].id, "The second local account.");
    CcSimAdvanceDays(&sim, 1);
    CcGossip swap = sim.gossip[0];
    sim.gossip[0] = sim.gossip[1];
    sim.gossip[1] = swap;
    CcSimAdvanceDays(&sim, 5);
    CC_CHECK(Account(first)->recorded);
    CC_CHECK(!Account(second)->recorded);
}

static void CheckLocalRumorText(void)
{
    static CcMetagame game;
    char output[8192];
    Prepare();
    const char *account = "The eastern guild offers silver bells.";
    AddAccount(sim.settlements[2].id, account);
    CcSimAdvanceDays(&sim, 1);
    CcMetagameInit(&game, 42U);
    game.sim = sim;
    CC_CHECK(CcMetagameExecute(&game, "rumors", output, sizeof(output)));
    CC_CHECK(strstr(output, account) == NULL);
    game.sim.player.location_id = sim.settlements[2].id;
    game.sim.carriage.location_id = game.sim.player.location_id;
    CC_CHECK(CcMetagameExecute(&game, "rumors", output, sizeof(output)));
    CC_CHECK(strstr(output, account) != NULL);

    CcId local = AddAccount(sim.settlements[1].id, "The local guild paints its hall.");
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(Account(local)->heard_day > 0);
    game.sim = sim;
    CC_CHECK(CcMetagameExecute(&game, "archives", output, sizeof(output)));
    CC_CHECK(strstr(output, "Accounts heard and awaiting ink:") != NULL);
    CC_CHECK(strstr(output, "Heard from Town residents") != NULL);
    CC_CHECK(strstr(output, "The local guild paints its hall.") != NULL);
}

static void CheckIncompleteSave(void)
{
    Prepare();
    AddAccount(sim.settlements[0].id, "A story waiting for a ride.");
    CcSimAdvanceDays(&sim, 1);
    const char *path = "gossip-incomplete.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "DELETE FROM gossip_account WHERE slot=0;",
                          NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(strstr(error, "Gossip rows") != NULL);
    (void)remove(path);
}

static void SetTellerBias(bool loyal)
{
    CcId faction_id = 0U;
    for (int32_t i = 0; i < sim.faction_count; ++i) {
        if (sim.factions[i].kingdom_id == sim.settlements[0].kingdom_id &&
            sim.factions[i].kind == (loyal ? CC_FACTION_CROWN : CC_FACTION_COMMONS)) {
            faction_id = sim.factions[i].id;
        }
    }
    CC_CHECK(faction_id != 0U);
    for (int32_t i = 0; i < sim.character_count; ++i) {
        CcCharacter *person = &sim.characters[i];
        if (person->current_settlement_id != sim.settlements[0].id) continue;
        person->role = loyal ? CC_CHARACTER_OFFICIAL : CC_CHARACTER_REFUGEE;
        person->faction_id = faction_id;
        person->stress = loyal ? 0 : 100;
        person->courage = loyal ? 100 : 0;
    }
}

static void CheckBiasAndDecay(void)
{
    char loyal_text[CC_EVENT_TEXT_CAPACITY];
    char fearful_text[CC_EVENT_TEXT_CAPACITY];
    const char *witness = "Raiders took three sacks from the western granary.";
    Prepare();
    SetTellerBias(true);
    CcId report = AddAccount(sim.settlements[0].id, witness);
    sim.events[sim.event_count - 1].kind = CC_EVENT_SETTLEMENT_RAIDED;
    Depart(sim.settlements[1].id);
    Arrive();
    CcGossipVersion loyal = Account(report)->heard;
    CcGossipText(&sim, Account(report), &loyal, loyal_text, sizeof(loyal_text));
    CC_CHECK(loyal.court_bias > 0 && loyal.source_character_id != 0U);
    CC_CHECK(strstr(loyal_text, "credit the crown") != NULL);
    CC_CHECK(Account(report)->local[0].confidence == 100);
    CC_CHECK(Account(report)->local[0].court_bias == 0);

    Prepare();
    SetTellerBias(false);
    report = AddAccount(sim.settlements[0].id, witness);
    sim.events[sim.event_count - 1].kind = CC_EVENT_SETTLEMENT_RAIDED;
    Depart(sim.settlements[1].id);
    Arrive();
    CcGossipVersion heard = Account(report)->heard;
    CcGossipText(&sim, Account(report), &heard, fearful_text, sizeof(fearful_text));
    CC_CHECK(heard.court_bias < 0);
    CC_CHECK(heard.confidence < loyal.confidence);
    CC_CHECK(heard.alarm > loyal.alarm);
    CC_CHECK(strcmp(fearful_text, loyal_text) != 0);
    CC_CHECK(strstr(fearful_text, "blame the court") != NULL);
    CC_CHECK(strstr(fearful_text, "five sacks") != NULL);
    CC_CHECK(strcmp(Account(report)->text, witness) == 0);
    CC_CHECK(Account(report)->local[0].retellings == 0);

    Depart(sim.settlements[2].id);
    Arrive();
    CC_CHECK(Account(report)->local[2].retellings > heard.retellings);
    CC_CHECK(Account(report)->local[2].confidence < heard.confidence);
    CC_CHECK(Account(report)->heard.confidence == heard.confidence);
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(Account(report)->recorded);
    bool found = false;
    for (int32_t i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (event->kind == CC_EVENT_LORE_RECORDED &&
            event->parent_id == Account(report)->heard_event_id) {
            CC_CHECK(strcmp(event->text, fearful_text) == 0);
            found = true;
        }
    }
    CC_CHECK(found);
    const char *path = "gossip-biased.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    (void)remove(path);
    CheckValid();
    printf("Loyal account: %s\nFearful account: %s\n", loyal_text, fearful_text);
}

static int32_t StoryOffset(const CcSim *s, CcId carrier, CcId event_id)
{
    for (int32_t o = 0; o < CC_MAX_GOSSIP; ++o) {
        const CcGossip *story = CcSimPersonalGossip(s, carrier, o, NULL);
        if (story != NULL && story->event_id == event_id) return o;
    }
    return -1;
}

static void CheckPersonalAccounts(void)
{
    Prepare();
    CcCharacter *traveller = &sim.characters[0];
    CcCharacter *neighbour = &sim.characters[1];
    traveller->current_settlement_id = sim.settlements[0].id;
    neighbour->current_settlement_id = sim.settlements[2].id;
    traveller->activity = neighbour->activity = CC_CHARACTER_ACTIVITY_WORKING;
    CcId report = AddAccount(sim.settlements[0].id, "Raiders took three sacks from the western granary.");
    CcSimRefreshCharacterGossip(&sim);
    const CcGossipVersion *version = NULL;
    int32_t report_offset = StoryOffset(&sim, traveller->id, report);
    CC_CHECK(report_offset >= 0);
    CcSimPersonalGossip(&sim, traveller->id, report_offset, &version);
    CC_CHECK(CcSimPersonalGossip(&sim, neighbour->id, 0, NULL) == NULL);
    CC_CHECK(version != NULL && version->retellings == 1);
    CcSpeech original;
    CC_CHECK(CcSpeechGossip(&sim, traveller->id, report_offset, false, &original));
    CC_CHECK(original.speaker_id == traveller->id && original.source_event_id == report);
    CC_CHECK(strstr(original.text, "three sacks") != NULL);
    traveller->current_settlement_id = neighbour->current_settlement_id;
    CcSimRefreshCharacterGossip(&sim);
    CcSpeech carried, source;
    report_offset = StoryOffset(&sim, traveller->id, report);
    CC_CHECK(CcSpeechGossip(&sim, traveller->id, report_offset, false, &carried));
    CC_CHECK(strcmp(carried.text, original.text) == 0);
    report_offset = StoryOffset(&sim, neighbour->id, report);
    CC_CHECK(report_offset >= 0);
    CC_CHECK(CcSpeechGossip(&sim, neighbour->id, report_offset, true, &source));
    CC_CHECK(strstr(source.text, traveller->name) != NULL);
    CC_CHECK(CcSpeechGossip(&sim, neighbour->id, report_offset, false, &carried));
    CC_CHECK(strstr(carried.text, "five sacks") != NULL);
    CcCommand talk = {.kind = CC_COMMAND_EXCHANGE_GOSSIP, .target_id = neighbour->id};
    uint64_t before = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &talk, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    sim.player.location_id = neighbour->current_settlement_id;
    sim.carriage.location_id = sim.player.location_id;
    const char *path = "personal-gossip.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, &sim, &talk, error, sizeof(error)));
    report_offset = StoryOffset(&sim, sim.player.id, report);
    CC_CHECK(report_offset >= 0);
    CcSimPersonalGossip(&sim, sim.player.id, report_offset, &version);
    CC_CHECK(version->source_character_id == neighbour->id);
    before = CcSimHash(&sim);
    CC_CHECK(CcJournalApply(journal, &sim, &talk, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == before);
    CcSpeech saved;
    CC_CHECK(CcSpeechGossip(&restored, neighbour->id, report_offset, false, &saved));
    CC_CHECK(strcmp(saved.text, carried.text) == 0 && saved.audio_key == carried.audio_key);
    CC_CHECK(CcSimPersonalGossip(&restored, neighbour->id, -1, NULL) == NULL);
    CC_CHECK(CcSimPersonalGossip(&restored, neighbour->id, CC_MAX_GOSSIP, NULL) == NULL);
    (void)remove(path);
    CheckValid();

    Prepare();
    sim.schema_version = 45U;
    AddAccount(sim.settlements[0].id, "An older traveller brings three sacks.");
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimGossipCarrierCapacity(&sim) == CC_LEGACY_GOSSIP_CARRIERS);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimGossipCarrierCapacity(&restored) == CC_MAX_GOSSIP_CARRIERS);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    (void)remove(path);
}

/* A notice posted on a board is a fact: it enters the ledger at the offer
   settlement, residents carry it, and travelers move it between towns. */
static void CheckNoticePosting(void)
{
    Prepare();
    int32_t offer = -1;
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        if (sim.situations[i].status == CC_SITUATION_ACTIVE &&
            sim.situations[i].kind != CC_SITUATION_MONSTER_EXPEDITION &&
            CcSimSituationCanAccept(&sim, &sim.situations[i]) &&
            CcSimSituationOfferSettlementId(&sim, &sim.situations[i]) != 0U) {
            offer = i;
            break;
        }
    }
    CC_CHECK(offer >= 0);
    CcId town = CcSimSituationOfferSettlementId(&sim, &sim.situations[offer]);
    CcSimRefreshCharacterGossip(&sim);
    bool posted = false;
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        const CcEvent *notice = CcSimEvent(&sim, sim.gossip[i].event_id);
        posted = posted || (sim.gossip[i].event_id != 0U &&
            sim.gossip[i].kind == CC_EVENT_NOTICE_POSTED &&
            notice != NULL &&
            notice->subject_id == sim.situations[offer].id);
    }
    CC_CHECK(posted);
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        const CcEvent *notice = CcSimEvent(&sim, sim.gossip[i].event_id);
        if (sim.gossip[i].event_id == 0U ||
            sim.gossip[i].kind != CC_EVENT_NOTICE_POSTED || notice == NULL ||
            notice->subject_id != sim.situations[offer].id) continue;
        CC_CHECK(sim.gossip[i].origin_id == town);
        CC_CHECK(strstr(sim.gossip[i].text, "posts a notice") != NULL);
    }
    /* The posting stays a single fact across further gatherings. */
    uint64_t before = CcSimHash(&sim);
    CcSimRefreshCharacterGossip(&sim);
    CcSimRefreshCharacterGossip(&sim);
    CC_CHECK(CcSimHash(&sim) == before);
    /* A resident of the offer town carries the posting and can speak it. */
    CcCharacter *resident = NULL;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].current_settlement_id == town &&
            sim.characters[i].activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
            CcCharacterAgeYears(&sim, &sim.characters[i]) >= 16) {
            resident = &sim.characters[i];
            break;
        }
    }
    CC_CHECK(resident != NULL);
    bool carried = false;
    for (int32_t o = 0; o < CC_MAX_GOSSIP; ++o) {
        const CcGossip *story = CcSimPersonalGossip(&sim, resident->id, o, NULL);
        const CcEvent *notice = story != NULL ? CcSimEvent(&sim, story->event_id) : NULL;
        carried = carried || (story != NULL &&
            story->kind == CC_EVENT_NOTICE_POSTED && notice != NULL &&
            notice->subject_id == sim.situations[offer].id);
    }
    CC_CHECK(carried);
}

/* Telling a story is a command: the told bits persist, the next untold story
   is offered first, and a ledger cannot claim a story nobody carries. */
static void CheckToldStories(void)
{
    Prepare();
    CcCharacter *speaker = &sim.characters[0];
    speaker->current_settlement_id = sim.settlements[0].id;
    speaker->activity = CC_CHARACTER_ACTIVITY_WORKING;
    CcId first = AddAccount(sim.settlements[0].id, "Raiders took three sacks from the western granary.");
    CcId second = AddAccount(sim.settlements[0].id, "The beacon fire burned all night.");
    CcSimRefreshCharacterGossip(&sim);
    const CcGossipVersion *version = NULL;
    int32_t slot = CcSimNextUntoldStory(&sim, speaker->id, &version);
    /* The offer-town posting is the freshest fact and is offered first. */
    CC_CHECK(slot >= 0 && sim.gossip[slot].kind == CC_EVENT_NOTICE_POSTED);
    CC_CHECK(!CcSimStoryTold(&sim, speaker->id, slot));
    CcCommand heard = {.kind = CC_COMMAND_HEARD_STORY,
        .target_id = speaker->id, .amount = slot};
    CC_CHECK(CcSimApply(&sim, &heard, error, sizeof(error)));
    CC_CHECK(CcSimStoryTold(&sim, speaker->id, slot));
    /* With the posting told, the newer account comes before the older one. */
    int32_t next = CcSimNextUntoldStory(&sim, speaker->id, &version);
    CC_CHECK(next >= 0 && sim.gossip[next].event_id == second);
    CC_CHECK(!CcSimStoryTold(&sim, speaker->id, next));
    CC_CHECK(CcSimApply(&sim, &((CcCommand){.kind = CC_COMMAND_HEARD_STORY,
        .target_id = speaker->id, .amount = next}), error, sizeof(error)));
    CC_CHECK(CcSimNextUntoldStory(&sim, speaker->id, &version) >= 0 &&
        sim.gossip[CcSimNextUntoldStory(&sim, speaker->id, NULL)].event_id == first);
    /* The telling is journalled; the save round trip keeps the bits. */
    const char *path = "told-stories.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, &sim, &heard, error, sizeof(error)));
    uint64_t told_hash = CcSimHash(&sim);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == told_hash);
    CC_CHECK(CcSimStoryTold(&restored, speaker->id, slot));
    CC_CHECK(CcSimStoryTold(&restored, speaker->id, next));
    CC_CHECK(!CcSimStoryTold(&restored, speaker->id, CcSimNextUntoldStory(
        &restored, speaker->id, NULL)));
    /* A told bit without a carried story is rejected. */
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        if (sim.gossip_carriers[i].id == speaker->id) {
            sim.gossip_carriers[i].told_player |=
                ~(sim.gossip_carriers[i].stories);
        }
    }
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    (void)remove(path);
}

int main(void)
{
    CheckPersonalAccounts();
    CheckNoticePosting();
    CheckToldStories();
    CheckLocalAndRemoteAccounts();
    CheckArrivalAndLateRecording();
    CheckCourierRelay();
    CheckLostCourier();
    CheckJournalAndLegacyReplay();
    CheckRelayAndBlockedRoad();
    CheckStorySlotReuse();
    CheckHearingOrder();
    CheckLocalRumorText();
    CheckIncompleteSave();
    CheckBiasAndDecay();
    puts("Traveler gossip network passed.");
    return 0;
}
