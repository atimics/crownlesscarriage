/* Census of an unattended simulation: what happens, how often, and who lives there.
 *
 * Runs N seeded worlds for D days with no player input and counts every event
 * kind, plus the population by role, occupation, goal and activity. Output is
 * JSON keyed by event kind number; tools/dialogue/sim_census.py names the kinds.
 */
#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define KINDS 512

static CcSim sim;

static void Array(const char *name, const double *values, int count, int worlds)
{
    printf("\"%s\":[", name);
    for (int i = 0; i < count; ++i) printf("%s%.2f", i ? "," : "", values[i] / worlds);
    printf("]");
}

int main(int argc, char **argv)
{
    int seeds = argc > 1 ? atoi(argv[1]) : 10, days = argc > 2 ? atoi(argv[2]) : 365;
    if (seeds < 1 || days < 1 || days > 36500) return 2;
    static double kinds[KINDS], role[16], occupation[16], goal[8], activity[8];
    double per_world[13] = {0};
    for (int seed = 1; seed <= seeds; ++seed) {
        CcSimInit(&sim, (uint32_t)seed);
        CcId seen = 0U;
        for (int day = 0; day < days; ++day) {
            CcSimAdvanceDays(&sim, 1);
            CcId top = seen;
            for (int32_t i = 0; i < sim.event_count; ++i) {
                const CcEvent *event = &sim.events[i];
                if (event->id <= seen) continue;
                if ((int)event->kind >= 0 && (int)event->kind < KINDS) kinds[event->kind] += 1.0;
                if (event->id > top) top = event->id;
            }
            seen = top;
        }
        const double counts[13] = {sim.character_count, sim.settlement_count, sim.kingdom_count, sim.faction_count,
            sim.bandit_count, sim.monster_count, sim.dungeon_count, sim.war_party_count, sim.courier_count,
            sim.shipment_count, sim.royal_carriage_count, sim.front_count, sim.situation_count};
        for (int i = 0; i < 13; ++i) per_world[i] += counts[i];
        for (int32_t i = 0; i < sim.character_count; ++i) {
            const CcCharacter *p = &sim.characters[i];
            if ((int)p->role >= 0 && (int)p->role < 16) role[p->role] += 1.0;
            if ((int)p->occupation >= 0 && (int)p->occupation < 16) occupation[p->occupation] += 1.0;
            if ((int)p->goal >= 0 && (int)p->goal < 8) goal[p->goal] += 1.0;
            if ((int)p->activity >= 0 && (int)p->activity < 8) activity[p->activity] += 1.0;
        }
    }
    static const char *const names[13] = {"people", "settlements", "kingdoms", "factions", "bandit_groups",
        "monsters", "dungeons", "war_parties", "couriers", "shipments", "royal_carriages", "fronts", "situations"};
    printf("{\"worlds\":%d,\"days\":%d,\"per_world\":{", seeds, days);
    for (int i = 0; i < 13; ++i) printf("%s\"%s\":%.2f", i ? "," : "", names[i], per_world[i] / seeds);
    printf("},");
    Array("role", role, 8, seeds); printf(",");
    Array("occupation", occupation, 12, seeds); printf(",");
    Array("goal", goal, 4, seeds); printf(",");
    Array("activity", activity, 6, seeds);
    printf(",\"events\":{");
    int first = 1;
    for (int k = 0; k < KINDS; ++k)
        if (kinds[k] > 0.0) { printf("%s\"%d\":%.3f", first ? "" : ",", k, kinds[k] / seeds); first = 0; }
    printf("}}\n");
    return 0;
}
