#include "sim/cc_road_council.h"
#include "sim/cc_production.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcSim base, trial;
static char error[256];

static bool Action(CcCommandKind kind, CcId target, int amount)
{
    CcCommand command = {.kind = kind, .target_id = target, .amount = amount};
    if (CcSimApply(&trial, &command, error, sizeof(error))) return true;
    fprintf(stderr, "action %d: %s\n", kind, error);
    return false;
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    int seed = atoi(argv[1]), age = atoi(argv[2]);
    if (seed < 1 || seed > 1000 || age < 0 || age > 1000) return 2;
    CcSimInit(&base, (uint32_t)seed * UINT32_C(2654435761));
    CcSimAdvanceDays(&base, age * 365);
    if (!CcSimValidate(&base, error, sizeof(error))) { fprintf(stderr, "baseline: %s\n", error); return 1; }
    /* Identical controlled arrival and food shock in every arm. */
    CcSettlement *town = &base.settlements[1];
    if (CcSettlementIsAbandoned(town) || !CcSettlementHasService(town, CC_SERVICE_BAKERY)) return 3;
    base.player.location_id = town->id;
    base.carriage.location_id = town->id;
    base.player.coins = 400;
    base.player.cargo_capacity = 18;
    memset(base.player.cargo, 0, sizeof(base.player.cargo));
    base.player.cargo[CC_GOOD_WHEAT] = 12;
    base.player.cargo[CC_GOOD_TOOLS] = 2;
    base.player.cargo[CC_GOOD_WOOD] = 2;
    base.player.cargo[CC_GOOD_STONE] = 2;
    /* One existing adult traveller arrives away from home with six crowns.
       Preserve their job, allegiance, memory, and ties. Set this once before branching. */
    CcId visitor_id = 0U;
    for (int i = 0; i < base.character_count; ++i) {
        CcCharacter *person = &base.characters[i];
        if (person->role != CC_CHARACTER_TRAVELLER || person->home_settlement_id == town->id ||
            person->death_day <= base.current_day || CcCharacterAgeYears(&base, person) < 16 ||
            person->bandit_group_id != 0U) continue;
        person->current_settlement_id = town->id;
        person->travel_coins = 6;
        visitor_id = person->id;
        break;
    }
    town->hunger = 45;
    for (int g = 0; g < CC_GOOD_COUNT; ++g)
        if (CcGoodNutritionValue((CcGood)g, CC_NUTRITION_CIVILIAN) > 0) town->stock[g] = 0;
    CcRoadCouncil initial = CcSimRoadCouncil(&base, town->id);
    if (initial.route_slot < 0) return 3;
    int road_slot = initial.route_slot;
    base.routes[road_slot].condition = 0;
    base.routes[road_slot].closed = true;
    if (!CcSimValidate(&base, error, sizeof(error))) { fprintf(stderr, "fixture: %s\n", error); return 1; }
    puts("seed,age,arm,day,hunger,population,nutrition,bread_made,road_condition,road_closed,active_loads,blocked_loads,other_units,delivered,lost,fund,spent,player_crowns,travellers_hungry,travellers_unsheltered,named_bandits,helpers_remembered,active_quests,resolved_quests,failed_quests,court,guild,commons,role_changes,visitor_id,visitor_here,visitor_bandit,visitor_role");
    const char *arms[] = {"wait", "fund", "supplies", "repair", "fund_repair"};
    for (int arm = 0; arm < 5; ++arm) {
        trial = base;
        if ((arm == 1 || arm == 4) && !Action(CC_COMMAND_FUND_GRAIN_SUPPLY, town->id, 0)) return 3;
        if (arm == 2 && !Action(CC_COMMAND_SUPPORT_BAKERY, town->id, 0)) return 3;
        if ((arm == 3 || arm == 4) && !Action(CC_COMMAND_REPAIR_ROUTE, base.routes[road_slot].id, 1)) return 3;
        /* Material repair takes one day. Every branch starts measurement on the same date. */
        CcSimAdvanceDays(&trial, base.current_day + 1 - trial.current_day);
        CcNutritionAccounting food = {0};
        CcProductionAccounting production = {0};
        for (int day = 0; day <= 180; ++day) {
            if (!CcSimValidate(&trial, error, sizeof(error))) { fprintf(stderr, "%s day %d: %s\n", arms[arm], day, error); return 1; }
            uint64_t nutrition = 0;
            for (int g = 0; g < CC_GOOD_COUNT; ++g)
                nutrition += food.towns[1].civilian_units[g] * (uint64_t)CcGoodNutritionValue((CcGood)g, CC_NUTRITION_CIVILIAN);
            int active = 0, blocked = 0, other = 0, hungry = 0, unsheltered = 0, bandits = 0, memory = 0, changed = 0;
            for (int i = 0; i < trial.shipment_count; ++i) {
                const CcShipment *s = &trial.shipments[i];
                if (s->status != CC_SHIPMENT_TRAVELLING && s->status != CC_SHIPMENT_BLOCKED) continue;
                if (s->origin_id != town->id && s->final_destination_id != town->id) continue;
                ++active; blocked += s->status == CC_SHIPMENT_BLOCKED;
                if (s->good != CC_GOOD_WHEAT) other += s->quantity;
            }
            for (int i = 0; i < trial.character_count; ++i) {
                const CcCharacter *p = &trial.characters[i];
                if (p->death_day <= trial.current_day || p->current_settlement_id != town->id) continue;
                bool traveller = (p->role == CC_CHARACTER_TRAVELLER || p->role == CC_CHARACTER_REFUGEE) && p->bandit_group_id == 0U;
                hungry += traveller && p->hungry_days >= 3;
                unsheltered += traveller && p->unsheltered_nights >= 3;
                bandits += p->bandit_group_id != 0U;
                memory += CcCharacterRemembers(p, CC_CHARACTER_MEMORY_PLAYER_HELPED, town->id);
                const CcCharacter *old = CcSimCharacter(&base, p->id);
                changed += old != NULL && old->role != p->role;
            }
            int quests[3] = {0}, support[3] = {0};
            for (int i = 0; i < trial.situation_count; ++i)
                if (CcSimSituationTouchesSettlement(&trial, &trial.situations[i], town->id)) ++quests[trial.situations[i].status];
            for (int i = 0; i < trial.faction_count; ++i)
                if (trial.factions[i].kingdom_id == town->kingdom_id) support[trial.factions[i].kind] = trial.factions[i].support;
            const CcGrainSupply *s = &trial.grain_supplies[1];
            const CcCharacter *visitor = CcSimCharacter(&trial, visitor_id);
            bool visitor_here = visitor != NULL && visitor->current_settlement_id == town->id && visitor->death_day > trial.current_day;
            if (getenv("CC_CONTESTED_TRACE") != NULL && visitor != NULL)
                fprintf(stderr, "%s,%d,%s,%d,%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%d,%d,%" PRIu64 "\n",
                    arms[arm], day, visitor->name, visitor->role, visitor->home_settlement_id,
                    visitor->current_settlement_id, visitor->travel_coins, visitor->hungry_days,
                    visitor->unsheltered_nights, visitor->bandit_group_id);
            printf("%d,%d,%s,%d,%d,%d,%" PRIu64 ",%" PRIu64 ",%d,%d,%d,%d,%d,%d,%d,%" PRId64 ",%" PRId64 ",%" PRId64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRIu64 ",%d,%d,%d\n",
                seed, age, arms[arm], day, trial.settlements[1].hunger, trial.settlements[1].population,
                nutrition, production.towns[1].bakery.output[CC_GOOD_BREAD], trial.routes[road_slot].condition,
                trial.routes[road_slot].closed, active, blocked, other, s->delivered, s->lost, s->purse, s->spent,
                trial.player.coins, hungry, unsheltered, bandits, memory, quests[0], quests[1], quests[2],
                support[0], support[1], support[2], changed, visitor_id, visitor_here,
                visitor != NULL && visitor->bandit_group_id != 0U, visitor != NULL ? (int)visitor->role : -1);
            if (day < 180) CcSimAdvanceDaysWithProductionAccounting(&trial, 1, &food, NULL, &production);
        }
    }
    return 0;
}
