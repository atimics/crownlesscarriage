#include "metagame/cc_metagame.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static int32_t SituationNumber(const CcMetagame *metagame,
                               CcSituationKind kind)
{
    for (int32_t i = 0; i < metagame->sim.situation_count; ++i) {
        if (metagame->sim.situations[i].status == CC_SITUATION_ACTIVE &&
            metagame->sim.situations[i].kind == kind) return i + 1;
    }
    return 0;
}

static CcSituation *Situation(CcMetagame *metagame,
                              CcSituationKind kind)
{
    for (int32_t i = 0; i < metagame->sim.situation_count; ++i) {
        if (metagame->sim.situations[i].kind == kind) {
            return &metagame->sim.situations[i];
        }
    }
    return NULL;
}

static int32_t EventCount(const CcMetagame *metagame, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < metagame->sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&metagame->sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

static void ExecuteNumber(CcMetagame *metagame, const char *verb,
                          int32_t number, char *output, size_t capacity)
{
    char command[64];
    (void)snprintf(command, sizeof(command), "%s %d", verb, number);
    CC_CHECK(CcMetagameExecute(metagame, command, output, capacity));
}

int main(void)
{
    char output[16384];
    char error[192];

    CcMetagame metagame;
    CcMetagameInit(&metagame, UINT32_C(42));
    CcMetagameIntro(&metagame, output, sizeof(output));
    CC_CHECK(strstr(output, "THE EMPTY GRANARY") != NULL);
    CC_CHECK(strstr(output, metagame.sim.settlements[1].name) != NULL);
    CC_CHECK(metagame.sim.player.location_id == metagame.sim.settlements[1].id);
    CC_CHECK(metagame.sim.player.coins == 75);

    CC_CHECK(CcMetagameExecute(&metagame, "causes", output, sizeof(output)));
    CC_CHECK(strstr(output, "short chain") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "rumors", output, sizeof(output)));
    CC_CHECK(strstr(output, "miller") != NULL);
    CC_CHECK(!CcMetagameExecute(&metagame, "plans", output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "people", output, sizeof(output)));
    CC_CHECK(strstr(output, metagame.sim.bandits[0].name) != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "inequality", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "Social fault lines") != NULL);
    CC_CHECK(strstr(output, "war burden") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "kingdoms", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "THE KINGDOMS OF MEN") != NULL);
    CC_CHECK(strstr(output, "Road and Granary") != NULL);
    CC_CHECK(strstr(output, "Iron and Wall") != NULL);
    CC_CHECK(strstr(output, "Capital and Deep") != NULL);
    CC_CHECK(strstr(output, "Politics (support / material power)") != NULL);
    CC_CHECK(strstr(output, "Present road strain") != NULL);
    for (int32_t i = 0; i < metagame.sim.kingdom_count; ++i) {
        CC_CHECK(strstr(output, metagame.sim.kingdoms[i].name) != NULL);
    }
    CC_CHECK(CcMetagameExecute(&metagame, "look", output, sizeof(output)));
    CC_CHECK(strstr(output, metagame.sim.kingdoms[0].name) != NULL);
    CC_CHECK(strstr(output, "Road and Granary realm") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "war", output, sizeof(output)));
    CC_CHECK(strstr(output, "Frontier roads") != NULL);
    CC_CHECK(strstr(output, "Crown Levy") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "charters", output, sizeof(output)));
    CC_CHECK(strstr(output, "Road compact") != NULL);
    CC_CHECK(strstr(output, "Quiet commission") != NULL);
    CC_CHECK(strstr(output, "Relief charter") == NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "routes", output, sizeof(output)));
    CC_CHECK(strstr(output, "no notes") != NULL);
    CC_CHECK(strstr(output, "unmarked track") != NULL);

    int32_t relief_number = SituationNumber(
        &metagame, CC_SITUATION_RELIEF_DELIVERY);
    char command[64];
    (void)snprintf(command, sizeof(command), "accept %d", relief_number);
    CC_CHECK(!CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(strstr(output, "sponsor is not here") != NULL);

    int32_t quiet_number = SituationNumber(
        &metagame, CC_SITUATION_BLACK_MARKET_DELIVERY);
    int32_t maps_before_commission = CcPlayerMapCount(&metagame.sim);
    ExecuteNumber(&metagame, "accept", quiet_number, output, sizeof(output));
    CC_CHECK(CcPlayerMapCount(&metagame.sim) == maps_before_commission);
    CC_CHECK(CcMetagameExecute(&metagame, "routes", output, sizeof(output)));
    CC_CHECK(strstr(output, "guide knows the turns") != NULL);

    CcSituation *quiet = Situation(
        &metagame, CC_SITUATION_BLACK_MARKET_DELIVERY);
    CC_CHECK(quiet != NULL);
    CcMoney coins_before_launder = metagame.sim.player.coins;
    CC_CHECK(CcMetagameExecute(&metagame, "buy food 8", output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "sell food 8", output, sizeof(output)));
    CC_CHECK(quiet->status == CC_SITUATION_ACTIVE);
    CC_CHECK(metagame.sim.player.coins < coins_before_launder);
    CC_CHECK(quiet->progress == 0);

    CC_CHECK(CcMetagameExecute(&metagame, "buy food 8", output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "travel 7", output, sizeof(output)));
    CC_CHECK(metagame.sim.journey.active);
    CC_CHECK(metagame.sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    CcMoney coins_before_bargain = metagame.sim.player.coins;
    CC_CHECK(CcMetagameExecute(&metagame, "road bargain", output,
                               sizeof(output)));
    CC_CHECK(metagame.sim.player.location_id ==
             metagame.sim.settlements[3].id);
    CC_CHECK(metagame.sim.player.coins < coins_before_bargain);
    CC_CHECK(CcMetagameExecute(&metagame, "sell food 8", output,
                               sizeof(output)));
    CC_CHECK(quiet->status == CC_SITUATION_RESOLVED);
    CC_CHECK(Situation(&metagame, CC_SITUATION_RELIEF_DELIVERY)->status ==
             CC_SITUATION_FAILED);
    CC_CHECK(Situation(&metagame, CC_SITUATION_ROUTE_REPAIR)->status ==
             CC_SITUATION_FAILED);
    CC_CHECK(metagame.sim.delayed_echo.active);

    const char *save_path = "/tmp/crownless-metagame-tests.ccsave";
    (void)remove(save_path);
    (void)snprintf(command, sizeof(command), "save %s", save_path);
    CC_CHECK(CcMetagameExecute(&metagame, command, output, sizeof(output)));
    int32_t saved_day = metagame.sim.current_day;
    CC_CHECK(CcMetagameExecute(&metagame, "wait 30", output, sizeof(output)));
    CC_CHECK(EventCount(&metagame, CC_EVENT_DELAYED_ECHO) == 1);
    CC_CHECK(metagame.sim.delayed_echo.active);
    const char *echo_path = "/tmp/crownless-metagame-echo-tests.ccsave";
    (void)remove(echo_path);
    (void)snprintf(command, sizeof(command), "save %s", echo_path);
    CC_CHECK(CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "wait 30", output, sizeof(output)));
    CC_CHECK(EventCount(&metagame, CC_EVENT_DELAYED_ECHO) == 2);
    CC_CHECK(!metagame.sim.delayed_echo.active);
    (void)snprintf(command, sizeof(command), "load %s", echo_path);
    CC_CHECK(CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(EventCount(&metagame, CC_EVENT_DELAYED_ECHO) == 1);
    CC_CHECK(metagame.sim.delayed_echo.active);
    CC_CHECK(CcMetagameExecute(&metagame, "wait 30", output, sizeof(output)));
    CC_CHECK(EventCount(&metagame, CC_EVENT_DELAYED_ECHO) == 2);
    CC_CHECK(!metagame.sim.delayed_echo.active);
    CC_CHECK(remove(echo_path) == 0);
    (void)snprintf(command, sizeof(command), "load %s", save_path);
    CC_CHECK(CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(metagame.sim.current_day == saved_day);
    CC_CHECK(remove(save_path) == 0);

    int32_t dungeon_number = SituationNumber(
        &metagame, CC_SITUATION_MONSTER_EXPEDITION);
    CC_CHECK(dungeon_number > 0);
    CC_CHECK(CcMetagameExecute(&metagame, "buy tools 2", output,
                               sizeof(output)));
    ExecuteNumber(&metagame, "accept", dungeon_number, output, sizeof(output));
    CC_CHECK(CcMetagameExecute(&metagame, "dungeon public", output,
                               sizeof(output)));
    CC_CHECK(metagame.sim.dungeons[0].state == CC_DUNGEON_PUBLIC_ROUTE);
    CC_CHECK(!metagame.sim.routes[6].smuggler_route);
    CC_CHECK(CcMetagameExecute(&metagame, "debrief", output, sizeof(output)));
    CC_CHECK(strstr(output, "Tell the story") != NULL);
    CC_CHECK(CcSimValidate(&metagame.sim, error, sizeof(error)));

    CcMetagame refusal;
    CcMetagameInit(&refusal, UINT32_C(77));
    int32_t repair_number = SituationNumber(
        &refusal, CC_SITUATION_ROUTE_REPAIR);
    CcSituation *repair = Situation(&refusal, CC_SITUATION_ROUTE_REPAIR);
    CC_CHECK(repair != NULL);
    int32_t issuer_support = 0;
    for (int32_t i = 0; i < refusal.sim.faction_count; ++i) {
        if (refusal.sim.factions[i].id == repair->issuer_faction_id) {
            issuer_support = refusal.sim.factions[i].support;
        }
    }
    ExecuteNumber(&refusal, "refuse", repair_number, output, sizeof(output));
    CC_CHECK(repair->status == CC_SITUATION_FAILED);
    for (int32_t i = 0; i < refusal.sim.faction_count; ++i) {
        if (refusal.sim.factions[i].id == repair->issuer_faction_id) {
            CC_CHECK(refusal.sim.factions[i].support == issuer_support - 2);
        }
    }

    CcMetagame fight;
    CcMetagameInit(&fight, UINT32_C(99));
    quiet_number = SituationNumber(&fight, CC_SITUATION_BLACK_MARKET_DELIVERY);
    ExecuteNumber(&fight, "accept", quiet_number, output, sizeof(output));
    CC_CHECK(CcMetagameExecute(&fight, "buy food 8", output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&fight, "travel 7", output, sizeof(output)));
    int32_t condition_before = fight.sim.carriage.condition;
    CcMoney fight_coins = fight.sim.player.coins;
    CC_CHECK(CcMetagameExecute(&fight, "road fight", output, sizeof(output)));
    CC_CHECK(fight.sim.carriage.condition < condition_before);
    CC_CHECK(fight.sim.player.coins < fight_coins);
    CC_CHECK(CcSimValidate(&fight.sim, error, sizeof(error)));

    CcMetagame dragon;
    CcMetagameInit(&dragon, UINT32_C(0xd12a60));
    dragon.sim.player.location_id = dragon.sim.dragon.lair_settlement_id;
    dragon.sim.carriage.location_id = dragon.sim.player.location_id;
    CC_CHECK(CcMetagameExecute(&dragon, "dragon", output, sizeof(output)));
    CC_CHECK(strstr(output, "only theft from the delivered hoard") != NULL);
    CC_CHECK(strstr(output, "Crowned dragon") != NULL);
    CC_CHECK(strstr(output, "Crown strength") != NULL);
    CC_CHECK(strstr(output, "Hoardkeepers") != NULL);
    CC_CHECK(CcMetagameExecute(&dragon, "economy", output, sizeof(output)));
    CC_CHECK(strstr(output, "Material economy") != NULL);
    CC_CHECK(strstr(output, "Weapons") != NULL);
    CC_CHECK(strstr(output, "mountain Iron") != NULL);

    CcSettlement *treasure_market = &dragon.sim.settlements[4];
    dragon.sim.goblins.tribute_cooldown_days = 1000;
    dragon.sim.hoard_raiders.cooldown_days = 1000;
    treasure_market->stock[CC_GOOD_GOLD] += 1;
    treasure_market->stock[CC_GOOD_GEMS] += 1;
    CcSimAdvanceDays(&dragon.sim, 21);
    int32_t treasure_number = 0;
    for (int32_t i = 0; i < dragon.sim.treasure_count; ++i) {
        if (dragon.sim.treasures[i].owner_id == treasure_market->id) {
            treasure_number = i + 1;
            break;
        }
    }
    CC_CHECK(treasure_number > 0);
    dragon.sim.player.location_id = treasure_market->id;
    dragon.sim.carriage.location_id = treasure_market->id;
    dragon.sim.player.coins = 500;
    CC_CHECK(CcMetagameExecute(&dragon, "treasures", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "for sale here") != NULL);
    ExecuteNumber(&dragon, "buy-treasure", treasure_number,
                  output, sizeof(output));
    CC_CHECK(dragon.sim.player.treasure_cargo_slots == 1);
    CC_CHECK(CcMetagameExecute(&dragon, "cargo", output, sizeof(output)));
    CC_CHECK(strstr(output, dragon.sim.treasures[treasure_number - 1].name) !=
             NULL);
    ExecuteNumber(&dragon, "sell-treasure", treasure_number,
                  output, sizeof(output));
    CC_CHECK(dragon.sim.player.treasure_cargo_slots == 0);

    dragon.sim.player.location_id = dragon.sim.dragon.lair_settlement_id;
    dragon.sim.carriage.location_id = dragon.sim.player.location_id;
    CC_CHECK(CcMetagameExecute(&dragon, "dragon steal 10",
                               output, sizeof(output)));
    CC_CHECK(strstr(output, "stolen crowns remain missing") != NULL);
    CC_CHECK(CcMetagameExecute(&dragon, "dragon return 10",
                               output, sizeof(output)));
    CC_CHECK(dragon.sim.dragon.stolen_outstanding == 0);
    CC_CHECK(strstr(output, "dragon is calm") != NULL);
    CcTreasure *remembered = &dragon.sim.treasures[treasure_number - 1];
    remembered->owner_id = dragon.sim.dragon.id;
    remembered->location_id = dragon.sim.dragon.lair_settlement_id;
    (void)snprintf(command, sizeof(command),
                   "dragon steal-treasure %d", treasure_number);
    CC_CHECK(CcMetagameExecute(&dragon, command, output, sizeof(output)));
    CC_CHECK(dragon.sim.dragon.stolen_treasure_id == remembered->id);
    CC_CHECK(strstr(output, "Only the exact object") != NULL);
    CC_CHECK(CcMetagameExecute(&dragon, "dragon return-treasure",
                               output, sizeof(output)));
    CC_CHECK(dragon.sim.dragon.stolen_treasure_id == 0U);
    CC_CHECK(remembered->owner_id == dragon.sim.dragon.id);

    puts("Text-first metagame tests passed");
    return 0;
}
