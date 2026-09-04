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
    int32_t blocked_shipments = 0;
    int32_t royal_idle = 0;
    int32_t royal_repositioning = 0;
    int32_t royal_delivering = 0;
    int32_t royal_blocked = 0;
    int32_t royal_waiting_capacity = 0;
    int32_t royal_trips = 0;
    int32_t royal_losses = 0;
    int32_t open_routes = 0;
    int32_t smuggler_routes = 0;
    int32_t legitimacy = 0;
    int32_t wars = 0;
    int32_t alliances = 0;
    int32_t active_couriers = 0;
    int32_t abandoned_settlements = 0;
    int32_t maximum_generation = 0;
    int32_t sanction = 0;
    int32_t anointed_count = 0;
    int32_t unsanctioned_weeks = 0;
    int32_t pretender_crises = 0;
    CcMoney debt = 0;
    const CcCharacter *abbot = CcSimCharacter(
        sim, sim->archives.abbot_character_id);
    const CcCharacter *campaign_patron = CcSimCharacter(
        sim, sim->dragon_campaign.patron_character_id);
    const CcCharacter *campaign_hero = CcSimCharacter(
        sim, sim->dragon_campaign.hero_character_id);
    const CcKingdom *anointed_kingdom = NULL;
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
        if (sim->shipments[i].status == CC_SHIPMENT_BLOCKED) {
            blocked_shipments += 1;
        }
    }
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *carriage = &sim->royal_carriages[i];
        if (carriage->mode == CC_ROYAL_CARRIAGE_IDLE) royal_idle += 1;
        if (carriage->mode == CC_ROYAL_CARRIAGE_REPOSITIONING) {
            royal_repositioning += 1;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING) {
            royal_delivering += 1;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED) royal_blocked += 1;
        if (carriage->mode == CC_ROYAL_CARRIAGE_WAITING_CAPACITY) {
            royal_waiting_capacity += 1;
        }
        royal_trips += carriage->trips_completed;
        royal_losses += carriage->cargo_losses;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (!sim->routes[i].closed) open_routes += 1;
        if (sim->routes[i].smuggler_route) smuggler_routes += 1;
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        legitimacy += sim->kingdoms[i].legitimacy;
        sanction += sim->kingdoms[i].sanction;
        if (sim->kingdoms[i].anointed) anointed_count += 1;
        if (sim->kingdoms[i].unsanctioned_weeks > unsanctioned_weeks) {
            unsanctioned_weeks = sim->kingdoms[i].unsanctioned_weeks;
        }
        pretender_crises += sim->kingdoms[i].pretender_crises;
        debt += sim->kingdoms[i].iron_ledger_debt;
        if (sim->kingdoms[i].anointed_by_character_id != 0U) {
            anointed_kingdom = &sim->kingdoms[i];
        }
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
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (sim->characters[i].generation > maximum_generation) {
            maximum_generation = sim->characters[i].generation;
        }
    }
    CcMaterialChainSnapshot chain = CcSimMaterialChainSnapshot(sim);
    (void)printf("day=%d hash=%016" PRIx64
                 " average_hunger=%d maximum_hunger=%d shipments=%d events=%d"
                 " blocked_shipments=%d royal_carriages=%d/%d/%d/%d/%d"
                 " royal_trips=%d royal_losses=%d"
                 " open_routes=%d/%d legitimacy=%d live_situations=%d"
                 " bandit_influence=%d monster_pressure=%d"
                 " night_roads=%d monastery_reserve=%" PRId64
                 " monastery_debt=%" PRId64
                 " hoard_raids=%d goblin_guards=%d goblin_members=%d"
                 " goblin_covenant=%d goblin_cohesion=%d goblin_tithes=%d"
                 " wars=%d alliances=%d couriers=%d"
                 " dragon_slain=%d dragon_campaign=%d/%d/%d"
                 " dragon_stage=%s activity=%s age=%d crown=%d body=%d"
                 " memory=%d territory=%d shadow=%d eggs=%d hunts=%d"
                 " broods=%d whelps=%d afterdeath=%d ruins=%d climate=%d"
                 " campaign_experience=%d"
                 " lore=%d lore_lost=%d scribes=%d"
                 " archive_chain=%s wheat=%d paper=%d tools=%d iron=%d"
                 " gold=%d gems=%d"
                 " inbound_tools=%d inbound_iron=%d"
                 " sanction=%d anointed=%d/%d unsanctioned_weeks=%d"
                 " pretender_crises=%d"
                 " people=%d births=%d deaths=%d generation=%d"
                 " abbot=\"%s\" stewardship=%d anointed=\"%s\""
                 " dragon_patron=\"%s\" dragon_hero=\"%s\""
                 " landless_days=%d\n",
                 sim->current_day, CcSimHash(sim),
                 total_hunger / sim->settlement_count, maximum_hunger,
                 travelling, sim->event_count,
                 blocked_shipments, royal_idle, royal_repositioning,
                 royal_delivering, royal_blocked, royal_waiting_capacity,
                 royal_trips, royal_losses,
                 open_routes, sim->route_count, legitimacy / sim->kingdom_count,
                 CcSimActiveSituationCount(sim),
                 sim->bandit_count > 0 ? sim->bandits[0].influence : 0,
                 sim->monster_count > 0 ? sim->monsters[0].pressure : 0,
                 smuggler_routes, sim->iron_ledger_reserve, debt,
                 sim->hoard_raiders.raids_completed,
                 sim->goblins.hoard_defenses, sim->goblins.members,
                 sim->goblins.devotion, sim->goblins.cohesion,
                 sim->goblins.tributes_delivered,
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
                 CcDragonCampaignExperience(sim),
                 sim->archives.lore_stored,
                 sim->archives.lore_lost_total,
                 sim->archives.scribes,
                 CcMaterialChainBlockerName(chain.blocker),
                 chain.wheat, chain.paper, chain.tools, chain.iron,
                 chain.gold, chain.gems,
                 chain.incoming_tools, chain.incoming_iron,
                 sanction / sim->kingdom_count, anointed_count,
                 sim->kingdom_count, unsanctioned_weeks,
                 pretender_crises,
                 sim->character_count, sim->character_births,
                 sim->character_deaths, maximum_generation,
                 abbot != NULL ? abbot->name : "vacant",
                 sim->archives.stewardship_rank,
                 anointed_kingdom != NULL ? anointed_kingdom->name : "none",
                 campaign_patron != NULL ? campaign_patron->name : "none",
                 campaign_hero != NULL ? campaign_hero->name : "none",
                 sim->dragon.territoryless_days);
    if (detail) {
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlement *place = &sim->settlements[i];
            (void)printf("  %-16s hunger=%3d prosperity=%3d security=%3d"
                         " stock=[bread:%3d iron:%3d tools:%3d weapons:%3d"
                         " gold:%3d gems:%3d wood:%3d wheat:%3d meat:%3d"
                         " wool:%3d stone:%3d paper:%3d]\n",
                         place->name, place->hunger, place->prosperity, place->security,
                         place->stock[CC_GOOD_FOOD], place->stock[CC_GOOD_MATERIAL],
                         place->stock[CC_GOOD_TOOLS],
                         place->stock[CC_GOOD_WEAPONS],
                         place->stock[CC_GOOD_GOLD], place->stock[CC_GOOD_GEMS],
                         place->stock[CC_GOOD_WOOD],
                         place->stock[CC_GOOD_WHEAT],
                         place->stock[CC_GOOD_MEAT],
                         place->stock[CC_GOOD_WOOL],
                         place->stock[CC_GOOD_STONE],
                         place->stock[CC_GOOD_PAPER]);
        }
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *person = &sim->characters[i];
            const CcSettlement *home = CcSimSettlement(
                sim, person->home_settlement_id);
            (void)printf("  %-22s age=%3d generation=%2d home=%s\n",
                         person->name, CcCharacterAgeYears(sim, person),
                         person->generation,
                         home != NULL ? home->name : "unknown");
        }
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            const CcRoyalCarriage *carriage = &sim->royal_carriages[i];
            (void)printf(
                "  royal[%d] mode=%s location=%llu route=%llu next=%llu"
                " target=%llu shipment=%llu depart=%d arrive=%d blocked=%d"
                " next_dispatch=%d condition=%d trips=%d losses=%d\n",
                i, CcRoyalCarriageModeName(carriage->mode),
                (unsigned long long)carriage->location_id,
                (unsigned long long)carriage->route_id,
                (unsigned long long)carriage->destination_id,
                (unsigned long long)carriage->target_id,
                (unsigned long long)carriage->active_shipment_id,
                carriage->departure_day, carriage->arrival_day,
                carriage->blocked_since_day, carriage->next_dispatch_day,
                carriage->condition, carriage->trips_completed,
                carriage->cargo_losses);
        }
    }
}
int main(int argc, char **argv)
{
    uint32_t seed = UINT32_C(0xc0a71a9e);
    int32_t years = 10;
    int32_t report_every = 1;
    const char *save_path = NULL;
    bool detail = false;
    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++argument], NULL, 0);
        } else if (strcmp(argv[argument], "--years") == 0 && argument + 1 < argc) {
            years = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--report-every") == 0 &&
                   argument + 1 < argc) {
            report_every = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--interval") == 0 &&
                   argument + 1 < argc) {
            report_every = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--save") == 0 && argument + 1 < argc) {
            save_path = argv[++argument];
        } else if (strcmp(argv[argument], "--detail") == 0) {
            detail = true;
        }
    }

    CcSim sim;
    CcSimInit(&sim, seed);
    char error[256];
    if (report_every < 1) report_every = 1;
    for (int32_t year = 0; year < years; ++year) {
        CcSimAdvanceDays(&sim, 365);
        if (!CcSimValidate(&sim, error, sizeof(error))) {
            (void)fprintf(stderr, "validation failed in year %d: %s\n", year + 1, error);
            if (detail) PrintSummary(&sim, true);
            return 1;
        }
        if (year == 0 || year + 1 == years ||
            (year + 1) % report_every == 0) {
            PrintSummary(&sim, detail);
        }
    }
    if (save_path != NULL && !CcSaveWrite(save_path, &sim, error, sizeof(error))) {
        (void)fprintf(stderr, "save failed: %s\n", error);
        return 1;
    }
    return 0;
}
