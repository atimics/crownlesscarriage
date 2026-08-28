#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintSummary(const CcSim *sim, bool detail)
{
    int32_t total_hunger = 0;
    int32_t maximum_hunger = 0;
    int32_t travelling = 0;
    int32_t open_routes = 0;
    int32_t smuggler_routes = 0;
    int32_t legitimacy = 0;
    int32_t wars = 0;
    int32_t alliances = 0;
    int32_t active_couriers = 0;
    int32_t abandoned_settlements = 0;
    CcMoney debt = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (CcSettlementIsAbandoned(&sim->settlements[i])) {
            abandoned_settlements += 1;
        }
        total_hunger += sim->settlements[i].hunger;
        if (sim->settlements[i].hunger > maximum_hunger) {
            maximum_hunger = sim->settlements[i].hunger;
        }
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING) travelling += 1;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (!sim->routes[i].closed) open_routes += 1;
        if (sim->routes[i].smuggler_route) smuggler_routes += 1;
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        legitimacy += sim->kingdoms[i].legitimacy;
        debt += sim->kingdoms[i].iron_ledger_debt;
        for (int32_t second = i + 1;
             second < sim->kingdom_count; ++second) {
            if (sim->diplomacy[i][second] == CC_DIPLOMACY_WAR) wars += 1;
            if (sim->diplomacy[i][second] == CC_DIPLOMACY_ALLIANCE) {
                alliances += 1;
            }
        }
    }
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourierStatus status = sim->couriers[i].status;
        if (status == CC_COURIER_WAITING ||
            status == CC_COURIER_TRAVELLING ||
            status == CC_COURIER_WITH_PLAYER) active_couriers += 1;
    }
    (void)printf("day=%d hash=%016" PRIx64
                 " average_hunger=%d maximum_hunger=%d shipments=%d events=%d"
                 " open_routes=%d/%d legitimacy=%d live_situations=%d"
                 " bandit_influence=%d monster_pressure=%d"
                 " night_roads=%d monastery_reserve=%" PRId64
                 " monastery_debt=%" PRId64
                 " hoard_raids=%d goblin_guards=%d goblin_members=%d"
                 " goblin_devotion=%d goblin_tithes=%d"
                 " wars=%d alliances=%d couriers=%d"
                 " dragon_slain=%d dragon_campaign=%d/%d/%d"
                 " dragon_stage=%s activity=%s age=%d crown=%d body=%d"
                 " memory=%d territory=%d shadow=%d eggs=%d hunts=%d"
                 " broods=%d whelps=%d afterdeath=%d ruins=%d climate=%d"
                 " campaign_experience=%d\n",
                 sim->current_day, CcSimHash(sim),
                 total_hunger / sim->settlement_count, maximum_hunger,
                 travelling, sim->event_count,
                 open_routes, sim->route_count, legitimacy / sim->kingdom_count,
                 CcSimActiveSituationCount(sim),
                 sim->bandit_count > 0 ? sim->bandits[0].influence : 0,
                 sim->monster_count > 0 ? sim->monsters[0].pressure : 0,
                 smuggler_routes, sim->iron_ledger_reserve, debt,
                 sim->hoard_raiders.raids_completed,
                 sim->goblins.hoard_defenses, sim->goblins.members,
                 sim->goblins.devotion, sim->goblins.tributes_delivered,
                 wars, alliances,
                 active_couriers, sim->dragon.slain ? 1 : 0,
                 sim->dragon_campaign.attempts,
                 sim->dragon_campaign.victories,
                 sim->dragon_campaign.defeats,
                 CcDragonLifeStageName(sim->dragon.life_stage),
                 CcDragonActivityName(sim->dragon.activity),
                 sim->dragon.age_days / 365,
                 sim->dragon.crown_strength,
                 sim->dragon.body_condition,
                 sim->dragon.memory_integrity,
                 sim->dragon.territory_stability,
                 sim->dragon.regional_influence,
                 sim->dragon.egg_count, sim->dragon.hunts,
                 sim->dragon.broods_laid,
                 sim->dragon.whelps_dispersed,
                 sim->dragon.afterdeath_days,
                 abandoned_settlements, CcSimClimateFactor(sim),
                 CcDragonCampaignExperience(sim));
    if (detail) {
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlement *place = &sim->settlements[i];
            (void)printf("  %-16s hunger=%3d prosperity=%3d security=%3d"
                         " stock=[%3d,%3d,%3d,%3d,%3d,%3d]"
                         " price=[%2d,%2d,%2d,%2d,%2d,%2d]\n",
                         place->name, place->hunger, place->prosperity, place->security,
                         place->stock[CC_GOOD_FOOD], place->stock[CC_GOOD_MATERIAL],
                         place->stock[CC_GOOD_TOOLS],
                         place->stock[CC_GOOD_WEAPONS],
                         place->stock[CC_GOOD_GOLD], place->stock[CC_GOOD_GEMS],
                         place->price[CC_GOOD_FOOD],
                         place->price[CC_GOOD_MATERIAL],
                         place->price[CC_GOOD_TOOLS],
                         place->price[CC_GOOD_WEAPONS],
                         place->price[CC_GOOD_GOLD], place->price[CC_GOOD_GEMS]);
        }
    }
}
int main(int argc, char **argv)
{
    uint32_t seed = UINT32_C(0xc0a71a9e);
    int32_t years = 10;
    const char *save_path = NULL;
    bool detail = false;
    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++argument], NULL, 0);
        } else if (strcmp(argv[argument], "--years") == 0 && argument + 1 < argc) {
            years = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--save") == 0 && argument + 1 < argc) {
            save_path = argv[++argument];
        } else if (strcmp(argv[argument], "--detail") == 0) {
            detail = true;
        }
    }

    CcSim sim;
    CcSimInit(&sim, seed);
    char error[256];
    for (int32_t year = 0; year < years; ++year) {
        CcSimAdvanceDays(&sim, 365);
        if (!CcSimValidate(&sim, error, sizeof(error))) {
            (void)fprintf(stderr, "validation failed in year %d: %s\n", year + 1, error);
            return 1;
        }
        PrintSummary(&sim, detail);
    }
    if (save_path != NULL && !CcSaveWrite(save_path, &sim, error, sizeof(error))) {
        (void)fprintf(stderr, "save failed: %s\n", error);
        return 1;
    }
    return 0;
}
