#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim_runner_json.inc"

static void PrintSummary(const CcSim *sim, bool detail)
{
    CcHungerSnapshot hunger = CcSimHungerSnapshot(sim);
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
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING) travelling += 1;
        if (sim->shipments[i].status == CC_SHIPMENT_BLOCKED) {
            blocked_shipments += 1;
        }
    }
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *carriage = &sim->royal_carriages[i];
        if (carriage->mode == CC_ROYAL_CARRIAGE_IDLE) royal_idle += 1;
        if (carriage->mode == CC_ROYAL_CARRIAGE_REPOSITIONING ||
            (carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING && carriage->active_shipment_id == 0)) {
            royal_repositioning += 1;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING ||
            (carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING && carriage->active_shipment_id != 0)) {
            royal_delivering += 1;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED ||
            carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING) royal_blocked += 1;
        if (carriage->mode == CC_ROYAL_CARRIAGE_WAITING_CAPACITY ||
            carriage->mode == CC_ROYAL_CARRIAGE_SITE_UNLOADING) {
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
    int32_t maximum_bandit_influence = 0, maximum_monster_pressure = 0;
    for (int32_t i = 0; i < sim->bandit_count; ++i)
        if (sim->bandits[i].influence > maximum_bandit_influence)
            maximum_bandit_influence = sim->bandits[i].influence;
    for (int32_t i = 0; i < sim->monster_count; ++i)
        if (sim->monsters[i].pressure > maximum_monster_pressure)
            maximum_monster_pressure = sim->monsters[i].pressure;
    char bandit_max[16] = "unavailable", monster_max[16] = "unavailable";
    if (sim->bandit_count > 0)
        (void)snprintf(bandit_max, sizeof(bandit_max), "%d", maximum_bandit_influence);
    if (sim->monster_count > 0)
        (void)snprintf(monster_max, sizeof(monster_max), "%d", maximum_monster_pressure);
    CcMaterialChainSnapshot chain = CcSimMaterialChainSnapshot(sim);
    (void)printf("day=%d hash=%016" PRIx64
                 " average_hunger=%d maximum_hunger=%d"
                 " population_weighted_hunger=%d inhabited_settlements=%d"
                 " hunger_population=%" PRId64 " shipments=%d events=%d"
                 " blocked_shipments=%d royal_carriages=%d/%d/%d/%d/%d"
                 " royal_trips=%d royal_losses=%d"
                 " open_routes=%d/%d legitimacy=%d live_situations=%d"
                 " bandit_groups=%d bandit_influence_max=%s"
                 " monster_groups=%d monster_pressure_max=%s"
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
                 hunger.average, hunger.maximum, hunger.population_weighted,
                 hunger.inhabited_settlements, hunger.population,
                 travelling, sim->event_count,
                 blocked_shipments, royal_idle, royal_repositioning,
                 royal_delivering, royal_blocked, royal_waiting_capacity,
                 royal_trips, royal_losses,
                 open_routes, sim->route_count, legitimacy / sim->kingdom_count,
                 CcSimActiveSituationCount(sim),
                 sim->bandit_count, bandit_max,
                 sim->monster_count, monster_max,
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
                 hunger.abandoned_settlements, CcSimClimateFactor(sim),
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
        CcRitualOfferingPlan offering = CcSimRitualOfferingPlan(sim);
        (void)printf("  ritual_offering_snapshot cult=%" PRIu64 " lair=%" PRIu64
            " phase=%d days_remaining=%d afterdeath_days=%d tribute_phase=%d"
            " existing_eggs=%d members=%d/48 devotion=%d/75 cohesion=%d/75"
            " coins=%" PRId64 "/120 relics=%d/2 food_rations=%d/12 tools=%d/2 weapons=%d/3"
            " planned_eggs=%d blocked=",
            sim->goblins.id, sim->goblins.lair_settlement_id,
            (int)sim->goblins.dragon_seed_phase, sim->goblins.dragon_seed_days_remaining,
            sim->dragon.afterdeath_days, (int)sim->goblins.tribute_phase, sim->dragon.egg_count,
            sim->goblins.members, sim->goblins.devotion, sim->goblins.cohesion,
            sim->goblins.lair_coins, offering.relics, offering.food_rations,
            sim->goblins.lair_stock[CC_GOOD_TOOLS], sim->goblins.lair_stock[CC_GOOD_WEAPONS],
            offering.eggs);
        bool offering_separator = false;
        for (unsigned bit = 0; bit < sizeof(RITUAL_BLOCK_NAMES) / sizeof(RITUAL_BLOCK_NAMES[0]); ++bit) {
            if ((offering.blocked & (UINT32_C(1) << bit)) == 0U) continue;
            (void)printf("%s%s", offering_separator ? "," : "", RITUAL_BLOCK_NAMES[bit]);
            offering_separator = true;
        }
        (void)puts(offering_separator ? "" : "ready");
        CcCampaignLaunchPlan launch = CcSimCampaignLaunchPlan(sim);
        (void)printf("  campaign_launch_snapshot phase=%d attempts=%d cooldown=%d"
            " pledged_mask=%" PRIu32 " pledges=%d/2 dragon_age_days=%d/182500"
            " dragon_stage=%d dragon_slain=%d prepare_eligible=%d"
            " held_food_rations=%d/32 held_tools=%d/8 held_weapons=%d/12"
            " patron=%" PRIu64 " hero=%" PRIu64 " origin=%" PRIu64 " blocked=",
            (int)sim->dragon_campaign.phase, sim->dragon_campaign.attempts,
            sim->dragon_campaign.cooldown_days, launch.pledged_mask, launch.pledged_count,
            sim->dragon.age_days, (int)sim->dragon.life_stage, sim->dragon.slain ? 1 : 0,
            (launch.blocked & CC_CAMPAIGN_PREPARATION_BLOCKS) == 0U ? 1 : 0,
            launch.food_rations, launch.tools, launch.weapons,
            launch.patron_id, launch.hero_id, launch.origin_id);
        bool launch_separator = false;
        for (unsigned bit = 0; bit < sizeof(CAMPAIGN_BLOCK_NAMES) / sizeof(CAMPAIGN_BLOCK_NAMES[0]); ++bit) {
            if ((launch.blocked & (UINT32_C(1) << bit)) == 0U) continue;
            (void)printf("%s%s", launch_separator ? "," : "", CAMPAIGN_BLOCK_NAMES[bit]);
            launch_separator = true;
        }
        (void)puts(launch_separator ? "" : "ready");
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
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRoute *route = &sim->routes[i];
            CcRoadRecoveryPlan plan = CcSimRoadRecoveryPlan(sim, route->id);
            (void)printf("  roadside_recovery route=%" PRIu64
                " from=%" PRIu64 " to=%" PRIu64 " closed=%d condition=%d"
                " day=%d next_work_day=%" PRId64 " labor_base=%" PRIu64
                " supplier=%" PRIu64 " people=%d/220 food_rations=%d/4"
                " wood=%d/2 stone=%d/2 tools=%d/1 effort=%d people_used=%d blocked=",
                route->id, route->from_id, route->to_id, route->closed ? 1 : 0,
                route->condition, sim->current_day, plan.next_work_day,
                plan.labor_base_id, plan.supplier_id, plan.population, plan.food_rations,
                plan.wood, plan.stone, plan.tools, plan.effort, plan.people_used);
            bool separator = false;
            for (unsigned bit = 0; bit < sizeof(ROAD_RECOVERY_BLOCK_NAMES) / sizeof(ROAD_RECOVERY_BLOCK_NAMES[0]); ++bit) {
                if ((plan.blocked & (UINT32_C(1) << bit)) == 0U) continue;
                (void)printf("%s%s", separator ? "," : "", ROAD_RECOVERY_BLOCK_NAMES[bit]);
                separator = true;
            }
            (void)puts(separator ? "" : "ready");
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
static bool chronicle = false;
static int32_t chronicle_last_day = -1;

static void PrintChronicleNewEvents(const CcSim *sim)
{
    int32_t count = sim->event_count;
    if (count > CC_MAX_EVENTS) count = CC_MAX_EVENTS;
    int32_t oldest_new = count;
    for (int32_t offset = count - 1; offset >= 0; --offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL) { oldest_new = offset; continue; }
        if (event->day <= chronicle_last_day) break;
        oldest_new = offset;
    }
    for (int32_t offset = oldest_new; offset >= 0; --offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->day <= chronicle_last_day) continue;
        (void)printf("  day=%-5d %-14s | %s\n",
                     event->day, CcEventKindName(event->kind), event->text);
    }
    const CcEvent *newest = CcSimRecentEvent(sim, 0);
    if (newest != NULL && newest->day > chronicle_last_day) {
        chronicle_last_day = newest->day;
    }
}

static void PrintSmithyAccounting(const CcSim *sim,
                                   const CcSmithyAccounting *accounting)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcTownSmithyAccounting *town = &accounting->towns[i];
        printf("smithy day=%d town=%" PRIu64 " tools=%" PRIu64 " weapons=%" PRIu64
               " iron_used=%" PRIu64 " wood_used=%" PRIu64 " tools_worn=%" PRIu64,
               sim->current_day, sim->settlements[i].id, town->tools_made,
               town->weapons_made, town->iron_used, town->wood_used, town->tools_worn);
        for (int32_t state = 0; state < CC_SMITHY_STATUS_COUNT; ++state) {
            printf(" tools_state_%d=%" PRIu64 " weapons_state_%d=%" PRIu64,
                   state, town->tools_status[state], state, town->weapons_status[state]);
        }
        putchar('\n');
    }
}

static void PrintRoadProduction(const CcSim *sim, const CcRoadProductionAccounting *accounting)
{
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcSiteProductionAccounting *row = &accounting->sites[i];
        printf("site day=%d id=%" PRIu64 " work=%" PRIu64 " repair=%" PRIu64,
            sim->current_day, sim->road_sites[i].id, row->work, row->route_repair);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            printf(" input_%d=%" PRIu64 " output_%d=%" PRIu64 " stock_%d=%d",
                good, row->input[good], good, row->output[good], good, sim->road_sites[i].stock[good]);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            if (row->freight_sent[good] || row->freight_received[good] || row->freight_delivered[good] ||
                row->freight_shipped[good] || row->freight_lost[good])
                printf(" sent_%d=%" PRIu64 " received_%d=%" PRIu64 " shipped_%d=%" PRIu64
                       " delivered_%d=%" PRIu64 " lost_%d=%" PRIu64,
                    good, row->freight_sent[good], good, row->freight_received[good],
                    good, row->freight_shipped[good], good, row->freight_delivered[good], good, row->freight_lost[good]);
        for (int32_t gate = 0; gate < CC_PRODUCTION_GATE_COUNT; ++gate)
            printf(" gate_%d=%" PRIu64, gate, row->gates[gate]);
        putchar('\n');
    }
}

static void PrintSiteFreight(const CcSim *sim)
{
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        for (int32_t j = 0; j < sim->road_site_count; ++j) {
            CcSiteFreightPlan plan = CcSimPlanSiteFreight(sim,
                sim->royal_carriages[i].id, sim->road_sites[j].id);
            if (plan.gate != CC_SITE_FREIGHT_READY) continue;
            printf("site-freight-preview day=%d carriage=%" PRIu64 " site=%" PRIu64
                   " town=%" PRIu64 " kind=%s good=%d quantity=%d outbound_days=%d return_days=%d\n",
                   sim->current_day, plan.carriage_id, plan.site_id, plan.town_id,
                   plan.kind == CC_SITE_FREIGHT_SUPPLY ? "supply" : "pickup",
                   (int)plan.good, plan.quantity, plan.travel_days, plan.return_days);
        }
    }
}

static bool ParseUnsignedArgument(const char *text, int base, uint32_t limit,
    uint32_t *value)
{
    if (text == NULL || text[0] < '0' || text[0] > '9') return false;
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, base);
    if (errno == ERANGE || end == text || *end != '\0' || parsed > limit) return false;
    *value = (uint32_t)parsed;
    return true;
}

int main(int argc, char **argv)
{
    uint32_t seed = UINT32_C(0xc0a71a9e);
    int32_t years = 10;
    int32_t report_every = 1;
    int32_t checkpoint_every = 0;
    const char *load_path = NULL;
    const char *save_path = NULL;
    bool json_report = false;
    bool opened_pilots = false;
    bool dragon_slain_day_one = false;
    CcNutritionAccounting nutrition = {0};
    int32_t route_open_days[CC_MAX_ROUTES] = {0};
    bool detail = false;
    bool smithy_report = false;
    bool site_report = false;
    bool site_freight_report = false;
    CcRoadProductionAccounting sites = {0};
    CcSmithyAccounting smithy = {0};
    for (int argument = 1; argument < argc; ++argument) {
        uint32_t parsed = 0;
        const char *option = argv[argument];
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            if (!ParseUnsignedArgument(argv[++argument], 0, UINT32_MAX, &seed)) {
                (void)fprintf(stderr, "Invalid value for %s: %s\n", option, argv[argument]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[argument], "--years") == 0 && argument + 1 < argc) {
            if (!ParseUnsignedArgument(argv[++argument], 10, INT32_MAX, &parsed)) {
                (void)fprintf(stderr, "Invalid value for %s: %s\n", option, argv[argument]);
                return EXIT_FAILURE;
            }
            years = (int32_t)parsed;
        } else if (strcmp(argv[argument], "--report-every") == 0 &&
                   argument + 1 < argc) {
            if (!ParseUnsignedArgument(argv[++argument], 10, INT32_MAX, &parsed)) {
                (void)fprintf(stderr, "Invalid value for %s: %s\n", option, argv[argument]);
                return EXIT_FAILURE;
            }
            report_every = (int32_t)parsed;
        } else if (strcmp(argv[argument], "--interval") == 0 &&
                   argument + 1 < argc) {
            if (!ParseUnsignedArgument(argv[++argument], 10, INT32_MAX, &parsed)) {
                (void)fprintf(stderr, "Invalid value for %s: %s\n", option, argv[argument]);
                return EXIT_FAILURE;
            }
            report_every = (int32_t)parsed;
        } else if (strcmp(argv[argument], "--save") == 0 && argument + 1 < argc) {
            save_path = argv[++argument];
        } else if (strcmp(argv[argument], "--load") == 0 && argument + 1 < argc) {
            load_path = argv[++argument];
        } else if (strcmp(argv[argument], "--checkpoint-every") == 0 &&
                   argument + 1 < argc) {
            if (!ParseUnsignedArgument(argv[++argument], 10, INT32_MAX, &parsed)) {
                (void)fprintf(stderr, "Invalid value for %s: %s\n", option, argv[argument]);
                return EXIT_FAILURE;
            }
            checkpoint_every = (int32_t)parsed;
        } else if (strcmp(argv[argument], "--json") == 0) {
            json_report = true;
        } else if (strcmp(argv[argument], "--dragon-slain-day-one") == 0) {
            dragon_slain_day_one = true;
        } else if (strcmp(argv[argument], "--opened-production-pilots") == 0) {
            opened_pilots = true;
        } else if (strcmp(argv[argument], "--detail") == 0) {
            detail = true;
        } else if (strcmp(argv[argument], "--site-freight") == 0) {
            site_freight_report = true;
        } else if (strcmp(argv[argument], "--sites") == 0) {
            site_report = true;
        } else if (strcmp(argv[argument], "--smithy") == 0) {
            smithy_report = true;
        } else if (strcmp(argv[argument], "--chronicle") == 0) {
            chronicle = true;
        } else {
            (void)fprintf(stderr, "Unknown or incomplete option: %s\n", option);
            return EXIT_FAILURE;
        }
    }

    CcSim sim;
    char error[256];
    if ((json_report && chronicle) || ((opened_pilots || dragon_slain_day_one) && load_path != NULL)) {
        fputs("Choose JSON or chronicle output; use a fresh seed for controlled fixtures.\n", stderr);
        return 1;
    }
    if (json_report) { smithy_report = true; site_report = true; }
    if (checkpoint_every < 0 || (checkpoint_every > 0 && save_path == NULL)) {
        (void)fprintf(stderr, "checkpoint interval requires --save and a positive year count\n");
        return 1;
    }
    if (load_path != NULL) {
        if (!CcSaveRead(load_path, &sim, error, sizeof(error))) {
            (void)fprintf(stderr, "load failed: %s\n", error);
            return 1;
        }
    } else {
        CcSimInit(&sim, seed);
    }
    if (opened_pilots) {
        const int32_t indices[] = {2, 11, 15};
        const CcRoadSiteKind kinds[] = {CC_ROAD_SITE_MILL, CC_ROAD_SITE_SMITHY, CC_ROAD_SITE_FARM};
        for (int32_t i = 0; i < 3; ++i) {
            CcRoadSite *site = &sim.road_sites[indices[i]];
            CcProductionRecipe recipe;
            if (site->kind != kinds[i] || !CcRoadSiteRecipe(site, &recipe)) {
                fputs("The production pilot requires its authored mill, forge and farm.\n", stderr);
                return 1;
            }
            site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
            site->stock[CC_GOOD_TOOLS] = 1;
            for (int32_t j = 0; j < recipe.input_count; ++j)
                site->stock[recipe.inputs[j].good] = recipe.inputs[j].reserve + 4 * recipe.inputs[j].units;
        }
    }
    if (dragon_slain_day_one) {
        sim.dragon.slain = true;
        sim.dragon.slain_day = 1;
        sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
        sim.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
        sim.dragon.body_condition = 0;
        sim.dragon.crown_strength = 0;
    }
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "initial validation failed: %s\n", error);
        return 1;
    }
    const char *dragon_policy = dragon_slain_day_one ? "slain-at-day-1" :
        load_path != NULL ? "loaded-save" : "natural-history";
    const int32_t start_day = sim.current_day;
    const char *fixture = opened_pilots ? "opened-production-pilots" : load_path != NULL ? "loaded-save" : "baseline";
    /* --years counts further years when resuming a saved world. */
    if (years < 0 || years > (INT32_MAX - sim.current_day) / 365) {
        (void)fprintf(stderr, "year count exceeds the simulation day range\n");
        return 1;
    }
    if (report_every < 1) report_every = 1;
    if (chronicle) {
        (void)printf("== world seed=%" PRIu32 " ==\n", sim.world_seed);
        for (int32_t i = 0; i < sim.kingdom_count; ++i) {
            (void)printf("kingdom: %s\n", sim.kingdoms[i].name);
        }
        for (int32_t i = 0; i < sim.settlement_count; ++i) {
            (void)printf("settlement: %s\n", sim.settlements[i].name);
        }
        PrintChronicleNewEvents(&sim);
    }
    if (json_report) PrintProductionJson(&sim, start_day, 0, fixture, dragon_policy, &nutrition, &smithy, &sites, route_open_days);
    for (int32_t year = 0; year < years; ++year) {
        if (json_report) {
            for (int32_t day = 0; day < 365; ++day) {
                CcSimAdvanceDaysWithProductionAccounting(&sim, 1, &nutrition, &smithy, &sites);
                for (int32_t i = 0; i < sim.route_count; ++i)
                    if (!sim.routes[i].closed) route_open_days[i]++;
            }
        } else if (chronicle) {
            /* Monthly scans: a busy year pushes more than the event ring
             * holds, so a yearly window would lose mid-year events. */
            for (int32_t month = 0; month < 12; ++month) {
                CcSimAdvanceDaysWithProductionAccounting(&sim, month == 11 ? 35 : 30,
                    NULL, smithy_report ? &smithy : NULL, site_report ? &sites : NULL);
                PrintChronicleNewEvents(&sim);
            }
        } else {
            CcSimAdvanceDaysWithProductionAccounting(&sim, 365, NULL,
                smithy_report ? &smithy : NULL, site_report ? &sites : NULL);
        }
        if (!CcSimValidate(&sim, error, sizeof(error))) {
            (void)fprintf(stderr, "validation failed in year %d: %s\n", year + 1, error);
            if (detail && !json_report) PrintSummary(&sim, true);
            return 1;
        }
        if (checkpoint_every > 0 && (year + 1) % checkpoint_every == 0 &&
            !CcSaveWrite(save_path, &sim, error, sizeof(error))) {
            (void)fprintf(stderr, "checkpoint failed: %s\n", error);
            return 1;
        }
        if (json_report) {
            if (year == 0 || year + 1 == years || (year + 1) % report_every == 0)
                PrintProductionJson(&sim, start_day, year + 1, fixture, dragon_policy, &nutrition, &smithy, &sites, route_open_days);
        } else if (chronicle) {
            (void)printf("== year %d (day %d) ==\n", year + 1, sim.current_day);
            PrintChronicleNewEvents(&sim);
            PrintSummary(&sim, detail);
            if (smithy_report) PrintSmithyAccounting(&sim, &smithy);
            if (site_report) PrintRoadProduction(&sim, &sites);
            if (site_freight_report) PrintSiteFreight(&sim);
        } else if (year == 0 || year + 1 == years ||
            (year + 1) % report_every == 0) {
            PrintSummary(&sim, detail);
            if (smithy_report) PrintSmithyAccounting(&sim, &smithy);
            if (site_report) PrintRoadProduction(&sim, &sites);
            if (site_freight_report) PrintSiteFreight(&sim);
            (void)fflush(stdout);
        }
    }
    if (save_path != NULL && !CcSaveWrite(save_path, &sim, error, sizeof(error))) {
        (void)fprintf(stderr, "save failed: %s\n", error);
        return 1;
    }
    return 0;
}
