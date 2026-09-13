#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static CcSim sim, before;
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    int seed = atoi(argv[1]), schema = atoi(argv[2]);
    if (seed < 1 || seed > 1000 || (schema != 73 && schema != 74)) return 2;
    CcSimInit(&sim, (uint32_t)seed * UINT32_C(2654435761));
    sim.schema_version = (uint32_t)schema;
    CcId town = sim.settlements[1].id, visitor_id = 0U;
    for (int i = 0; i < sim.character_count; ++i) {
        CcCharacter *person = &sim.characters[i];
        if (person->role != CC_CHARACTER_TRAVELLER || person->home_settlement_id == town ||
            person->death_day <= sim.current_day || CcCharacterAgeYears(&sim, person) < 16 ||
            person->bandit_group_id != 0U) continue;
        person->current_settlement_id = town;
        person->travel_coins = 6;
        visitor_id = person->id;
        break;
    }
    if (visitor_id == 0U) return 3;
    sim.settlements[1].hunger = 45;
    for (int g = 0; g < CC_GOOD_COUNT; ++g)
        if (CcGoodNutritionValue((CcGood)g, CC_NUTRITION_CIVILIAN) > 0) sim.settlements[1].stock[g] = 0;
    char error[256];
    int moved = 0, roles = 0, allegiance = 0, created = 0;
    puts("seed,schema,day,visitor_id,visitor_here,visitor_role,visitor_bandit,hunger,active_quests,new_quests,person_moves,role_changes,allegiance_changes,hash");
    for (int day = 0; day <= 180; ++day) {
        if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%d:%d %s\n", schema, day, error); return 1; }
        const CcCharacter *visitor = CcSimCharacter(&sim, visitor_id);
        printf("%d,%d,%d,%" PRIu64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRIu64 "\n",
            seed, schema, day, visitor_id, visitor != NULL && visitor->current_settlement_id == town,
            visitor != NULL ? (int)visitor->role : -1, visitor != NULL && visitor->bandit_group_id != 0U,
            sim.settlements[1].hunger, CcSimActiveSituationCount(&sim), created, moved, roles, allegiance, CcSimHash(&sim));
        if (day == 180) break;
        before = sim;
        CcSimAdvanceDays(&sim, 1);
        for (int i = 0; i < sim.character_count; ++i) {
            const CcCharacter *person = &sim.characters[i];
            const CcCharacter *old = CcSimCharacter(&before, person->id);
            if (old == NULL) continue;
            moved += old->current_settlement_id != person->current_settlement_id;
            roles += old->role != person->role;
            allegiance += old->faction_id != person->faction_id;
        }
        for (int i = 0; i < sim.situation_count; ++i) {
            const CcSituation *quest = &sim.situations[i];
            if (quest->created_day == sim.current_day) ++created;
        }
    }
    return 0;
}
