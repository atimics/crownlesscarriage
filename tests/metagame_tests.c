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
    CC_CHECK(strstr(output, "THE ROAD WITHOUT A CROWN") != NULL);
    CC_CHECK(strstr(output,
                    "It is never a good sign when the first bell rings") !=
             NULL);
    CC_CHECK(strstr(output, "on foot") != NULL);
    CC_CHECK(strstr(output, "no carriage") != NULL);
    CC_CHECK(strstr(output, "Nell Varo") != NULL);
    CC_CHECK(strstr(output, "Not a crown. A wheel.") != NULL);
    CC_CHECK(strstr(output, metagame.sim.settlements[0].name) != NULL);
    CC_CHECK(metagame.sim.player.location_id == metagame.sim.settlements[0].id);
    CC_CHECK(metagame.sim.player.coins == 75);

    CcMetagame goblin_ui;
    CcMetagameInit(&goblin_ui, UINT32_C(0x60b11d));
    CC_CHECK(CcMetagameExecute(
        &goblin_ui, "goblins", output, sizeof(output)));
    CC_CHECK(strstr(output, "Nara Soot-Tongue") != NULL);
    CC_CHECK(strstr(output, "covenant") != NULL);
    CcSettlement *goblin_lair = CcSimSettlementMutable(
        &goblin_ui.sim, goblin_ui.sim.goblins.lair_settlement_id);
    CC_CHECK(goblin_lair != NULL);
    goblin_ui.sim.player.location_id = goblin_lair->id;
    goblin_ui.sim.carriage.location_id = goblin_lair->id;
    goblin_ui.sim.player.cargo[CC_GOOD_FOOD] = 2;
    goblin_lair->market_coins = 200;
    CC_CHECK(CcMetagameExecute(
        &goblin_ui, "goblins trade food 2", output, sizeof(output)));
    CC_CHECK(strstr(output, "Lair stores") != NULL);
    goblin_ui.sim.current_day = 6;
    goblin_ui.sim.goblins.tribute_cooldown_days = 0;
    CcSimAdvanceDays(&goblin_ui.sim, 1);
    CcId goblin_target = goblin_ui.sim.goblins.tribute_target_id;
    CC_CHECK(goblin_target != 0U);
    goblin_ui.sim.player.location_id = goblin_target;
    goblin_ui.sim.carriage.location_id = goblin_target;
    CC_CHECK(CcMetagameExecute(
        &goblin_ui, "goblins warn", output, sizeof(output)));
    CC_CHECK(strstr(output, "has been warned") != NULL);
    CC_CHECK(CcMetagameExecute(
        &goblin_ui, "goblins intercept", output, sizeof(output)));
    CC_CHECK(strstr(output, "No expedition is active") != NULL);

    CC_CHECK(CcMetagameExecute(&metagame, "causes", output, sizeof(output)));
    CC_CHECK(strstr(output, "short chain") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "rumors", output, sizeof(output)));
    CC_CHECK(strstr(output, "ravens") != NULL);
    CC_CHECK(strstr(output, "black wax") != NULL);
    CC_CHECK(!CcMetagameExecute(&metagame, "plans", output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "people", output, sizeof(output)));
    CC_CHECK(strstr(output, "one red mitten") != NULL);
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
    CC_CHECK(strstr(output, "sleeping beetles") != NULL);
    CC_CHECK(strstr(output, "mossy milestone") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "war", output, sizeof(output)));
    CC_CHECK(strstr(output, "Frontier roads") != NULL);
    CC_CHECK(strstr(output, "Crown Levy") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "charters", output, sizeof(output)));
    CC_CHECK(strstr(output, "Mara Venn's white-wax letter") != NULL);
    CC_CHECK(strstr(output, "foxfire supper") == NULL);
    int32_t relief_number = SituationNumber(
        &metagame, CC_SITUATION_RELIEF_DELIVERY);
    ExecuteNumber(&metagame, "talk", relief_number, output, sizeof(output));
    CC_CHECK(strstr(output, "empty bowl is a meal") != NULL);
    CC_CHECK(strstr(output, "Nell's grain") != NULL);
    CC_CHECK(strstr(output, "eight sacks of flour for Silverwick") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "routes", output, sizeof(output)));
    CC_CHECK(strstr(output, "Baker's Road") != NULL);

    int32_t quiet_number = SituationNumber(
        &metagame, CC_SITUATION_BLACK_MARKET_DELIVERY);
    char command[64];
    (void)snprintf(command, sizeof(command), "accept %d", quiet_number);
    CC_CHECK(!CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(strstr(output, "sponsor is not here") != NULL);

    CC_CHECK(CcMetagameExecute(&metagame, "travel 1", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "headless fountain") != NULL);
    CC_CHECK(metagame.sim.player.location_id ==
             metagame.sim.settlements[1].id);
    CC_CHECK(CcMetagameExecute(&metagame, "charters", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "Ilyra Senn's iron chain") != NULL);
    CC_CHECK(strstr(output, "Tomas Rill's foxfire supper") != NULL);
    ExecuteNumber(&metagame, "talk", quiet_number, output, sizeof(output));
    CC_CHECK(strstr(output, "green scarf") != NULL);
    CC_CHECK(strstr(output, "boiled their seed grain") != NULL);
    CC_CHECK(strstr(output, "No soldiers. No inspections") != NULL);
    CC_CHECK(CcMetagameExecute(&metagame, "routes", output, sizeof(output)));
    CC_CHECK(strstr(output, "no notes") != NULL);
    CC_CHECK(strstr(output, "unmarked track") != NULL);

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
    CC_CHECK(CcMetagameExecute(&metagame, "history 3", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "A letter from Tomas Rill") != NULL);
    CC_CHECK(strstr(output, "onion soup") != NULL);
    const char *echo_path = "/tmp/crownless-metagame-echo-tests.ccsave";
    (void)remove(echo_path);
    (void)snprintf(command, sizeof(command), "save %s", echo_path);
    CC_CHECK(CcMetagameExecute(&metagame, command, output, sizeof(output)));
    CC_CHECK(CcMetagameExecute(&metagame, "wait 30", output, sizeof(output)));
    CC_CHECK(EventCount(&metagame, CC_EVENT_DELAYED_ECHO) == 2);
    CC_CHECK(!metagame.sim.delayed_echo.active);
    CC_CHECK(CcMetagameExecute(&metagame, "history 3", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "A second letter from Tomas Rill") != NULL);
    CC_CHECK(strstr(output, "Night Road collectors") != NULL);
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
    CC_CHECK(CcMetagameExecute(&metagame, "charters", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "Ask Jory Fen about the strange noises") != NULL);
    CC_CHECK(strstr(output, "No reward has been offered yet") != NULL);
    ExecuteNumber(&metagame, "talk", dungeon_number, output, sizeof(output));
    CC_CHECK(strstr(output, "Jory: \"") != NULL);
    CC_CHECK(strstr(output, "Ask Bren what happened in the mine") != NULL);
    ExecuteNumber(&metagame, "talk", dungeon_number, output, sizeof(output));
    CC_CHECK(strstr(output, "using a pick behind the old wall") != NULL);
    CC_CHECK(strstr(output, "Tell Jory what Bren heard") != NULL);
    ExecuteNumber(&metagame, "talk", dungeon_number, output, sizeof(output));
    CC_CHECK(strstr(output, "tell") != NULL);
    ExecuteNumber(&metagame, "keep", dungeon_number,
                  output, sizeof(output));
    CC_CHECK(strstr(output, "help find Cera") != NULL);
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

    CcMetagame lawful;
    CcMetagameInit(&lawful, UINT32_C(42));
    relief_number = SituationNumber(&lawful, CC_SITUATION_RELIEF_DELIVERY);
    ExecuteNumber(&lawful, "accept", relief_number, output, sizeof(output));
    CC_CHECK(strstr(output, "Mara Venn closes the brass charter box") != NULL);
    CC_CHECK(CcMetagameExecute(&lawful, "buy food 8", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&lawful, "travel 1", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&lawful, "travel 2", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "Captain Ilyra Senn") != NULL);
    CC_CHECK(CcMetagameExecute(&lawful, "road bargain", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "report will blame the bridge machinery") != NULL);
    CC_CHECK(CcMetagameExecute(&lawful, "travel 3", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "market clock is still waiting for breakfast") !=
             NULL);
    ExecuteNumber(&lawful, "talk", relief_number, output, sizeof(output));
    CC_CHECK(strstr(output, "mine owners stopped selling flour") != NULL);
    CC_CHECK(strstr(output, "Flour has never answered") == NULL);
    CC_CHECK(CcMetagameExecute(&lawful, "sell food 8", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "No golden light declares the choice good") !=
             NULL);
    CC_CHECK(lawful.sim.delayed_echo.active);
    CC_CHECK(CcMetagameExecute(&lawful, "wait 30", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&lawful, "history 5", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "A letter from Jory Fen") != NULL);
    CC_CHECK(strstr(output, "tied with red thread") != NULL);
    CC_CHECK(CcSimValidate(&lawful.sim, error, sizeof(error)));

    CcMetagame supper;
    CcMetagameInit(&supper, UINT32_C(42));
    CC_CHECK(CcMetagameExecute(&supper, "travel 1", output,
                               sizeof(output)));
    quiet_number = SituationNumber(
        &supper, CC_SITUATION_BLACK_MARKET_DELIVERY);
    ExecuteNumber(&supper, "accept", quiet_number, output, sizeof(output));
    CC_CHECK(CcMetagameExecute(&supper, "buy food 12", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&supper, "travel 7", output,
                               sizeof(output)));
    int32_t supper_food = supper.sim.player.cargo[CC_GOOD_FOOD];
    CC_CHECK(CcMetagameExecute(&supper, "road supper", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "one onion and a great deal of hope") != NULL);
    CC_CHECK(supper.sim.player.cargo[CC_GOOD_FOOD] < supper_food);
    CC_CHECK(CcSimValidate(&supper.sim, error, sizeof(error)));

    CcMetagame turn_back;
    CcMetagameInit(&turn_back, UINT32_C(42));
    turn_back.sim.player.location_id = turn_back.sim.settlements[1].id;
    turn_back.sim.carriage.location_id = turn_back.sim.player.location_id;
    quiet_number = SituationNumber(
        &turn_back, CC_SITUATION_BLACK_MARKET_DELIVERY);
    ExecuteNumber(&turn_back, "accept", quiet_number, output, sizeof(output));
    CC_CHECK(CcMetagameExecute(&turn_back, "buy food 8", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&turn_back, "travel 7", output,
                               sizeof(output)));
    CC_CHECK(CcMetagameExecute(&turn_back, "road turn-back", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "A road refused is still a choice") != NULL);
    CC_CHECK(turn_back.sim.player.location_id ==
             turn_back.sim.settlements[1].id);
    CC_CHECK(!turn_back.sim.journey.active);
    CC_CHECK(CcSimValidate(&turn_back.sim, error, sizeof(error)));

    CcMetagame refusal;
    CcMetagameInit(&refusal, UINT32_C(77));
    refusal.sim.player.location_id = refusal.sim.settlements[1].id;
    refusal.sim.carriage.location_id = refusal.sim.player.location_id;
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
    fight.sim.player.location_id = fight.sim.settlements[1].id;
    fight.sim.carriage.location_id = fight.sim.player.location_id;
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

    CcMetagame horses;
    CcMetagameInit(&horses, UINT32_C(0x57ab1e));
    horses.sim.player.location_id = horses.sim.settlements[0].id;
    horses.sim.carriage.location_id = horses.sim.player.location_id;
    CC_CHECK(CcMetagameExecute(&horses, "animals", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "traits: strength") != NULL);
    CC_CHECK(CcMetagameExecute(&horses, "stable breed 2 1", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "due in 330 days") != NULL);
    horses.sim.horse_team[1].pregnancy_days_remaining = 1;
    CC_CHECK(CcMetagameExecute(&horses, "wait 1", output,
                               sizeof(output)));
    CC_CHECK(horses.sim.stable_horse_count == 1);
    horses.sim.stable_horses[0].age_days = 3 * 365;
    horses.sim.stable_horses[0].training = 80;
    CC_CHECK(CcMetagameExecute(&horses, "stable team 1 3", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "team 1") != NULL);
    CC_CHECK(CcSimValidate(&horses.sim, error, sizeof(error)));

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
    dragon.sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
    dragon.sim.goblins.tribute_target_id =
        dragon.sim.dragon.lair_settlement_id;
    dragon.sim.goblins.tribute_days_remaining = 2;
    dragon.sim.goblins.carried_tribute = 7;
    CcMoney intercept_coins = dragon.sim.player.coins;
    CC_CHECK(CcMetagameExecute(&dragon, "dragon intercept",
                               output, sizeof(output)));
    CC_CHECK(dragon.sim.player.coins == intercept_coins + 7);
    CC_CHECK(dragon.sim.goblins.tribute_phase ==
             CC_GOBLIN_TRIBUTE_IDLE);

    puts("Text-first metagame tests passed");
    return 0;
}
