#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#include "test_support.h"
static CcSim fixture, before;
int main(void)
{
    const int counts[] = {0, 1, 3};
    for (int case_index = 0; case_index < 3; ++case_index) {
        CcSimInit(&fixture, 42);
        CcBanditGroup bandit = fixture.bandits[0];
        CcMonsterPopulation monster = fixture.monsters[0];
        fixture.bandit_count = counts[case_index];
        fixture.monster_count = counts[case_index];
        for (int i = 0; i < counts[case_index]; ++i) {
            fixture.bandits[i] = bandit; fixture.monsters[i] = monster;
            fixture.bandits[i].id = CcMakeId(CC_ENTITY_BANDIT_GROUP, fixture.next_entity_serial++);
            fixture.monsters[i].id = CcMakeId(CC_ENTITY_MONSTER_POPULATION, fixture.next_entity_serial++);
            fixture.bandits[i].influence = i == 1 ? 90 : 10 + i;
            fixture.monsters[i].pressure = i == 1 ? 80 : 20 + i;
            (void)snprintf(fixture.bandits[i].name, sizeof(fixture.bandits[i].name), "Group \"%d\"\n", i);
        }
        before = fixture;
        JsonThreatSnapshot(&fixture); putchar('\n');
        PrintSummary(&fixture, false);
        CC_CHECK(memcmp(&fixture, &before, sizeof(fixture)) == 0);
    }
    return 0;
}
