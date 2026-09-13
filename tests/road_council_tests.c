#include "sim/cc_road_council.h"
#include "metagame/cc_metagame.h"
#include "test_support.h"
#include <string.h>

static CcMetagame game;
static CcSim before;
int main(void)
{
    CcMetagameInit(&game, 42U);
    CcSim *sim = &game.sim;
    sim->player.location_id = sim->settlements[1].id;
    sim->carriage.location_id = sim->player.location_id;
    before = *sim;
    CcRoadCouncil council = CcSimRoadCouncil(sim, sim->player.location_id);
    CC_CHECK(council.settlement_id == sim->player.location_id && council.route_id != 0U);
    CC_CHECK(memcmp(sim, &before, sizeof(before)) == 0);
    CC_CHECK(CcSimRoadCouncil(NULL, 0).settlement_id == 0U);
    CC_CHECK(CcSimRoadCouncil(sim, 0).settlement_id == 0U);
    /* The view follows a real person as they leave, die, or join a camp. */
    CcCharacter *person = &sim->characters[0];
    person->current_settlement_id = sim->player.location_id;
    person->role = CC_CHARACTER_TRAVELLER;
    person->bandit_group_id = 0U;
    person->hungry_days = 7;
    person->unsheltered_nights = 7;
    person->death_day = sim->current_day + 100;
    council = CcSimRoadCouncil(sim, sim->player.location_id);
    CC_CHECK(council.rows[3].actor_id == person->id);
    person->bandit_group_id = sim->bandits[0].id;
    CC_CHECK(CcSimRoadCouncil(sim, sim->player.location_id).rows[3].actor_id != person->id);
    person->bandit_group_id = 0U;
    person->death_day = sim->current_day;
    CC_CHECK(CcSimRoadCouncil(sim, sim->player.location_id).rows[3].actor_id != person->id);
    *sim = before;
    /* Public work links to its actual quest and sponsor. Private work stays private. */
    int slot = -1;
    for (int i = 0; i < sim->situation_count; ++i) {
        CcSituation *quest = &sim->situations[i];
        if (quest->kind == CC_SITUATION_ROUTE_REPAIR && quest->target_id == council.route_id) slot = i;
    }
    CC_CHECK(slot >= 0);
    sim->posted_situation_mask = 0U;
    sim->player.accepted_situation_id = 0U;
    CC_CHECK(CcSimRoadCouncil(sim, sim->player.location_id).rows[1].situation_id == 0U);
    sim->posted_situation_mask |= UINT32_C(1) << (unsigned)slot;
    CC_CHECK(CcSimRoadCouncil(sim, sim->player.location_id).rows[1].situation_id == sim->situations[slot].id);
    before = *sim;
    char output[8192];
    CC_CHECK(CcMetagameExecute(&game, "council", output, sizeof(output)));
    CC_CHECK(strstr(output, "Road meeting") != NULL && strstr(output, "Public work") != NULL);
    CC_CHECK(memcmp(sim, &before, sizeof(before)) == 0);
    sim->player.cargo[CC_GOOD_TOOLS] = 2;
    sim->player.cargo[CC_GOOD_WOOD] = 2;
    sim->player.cargo[CC_GOOD_STONE] = 2;
    CcCommand repair = {.kind = CC_COMMAND_REPAIR_ROUTE, .target_id = council.route_id, .amount = 1};
    char error[256];
    CC_CHECK(CcSimApply(sim, &repair, error, sizeof(error)));
    CC_CHECK(sim->situations[slot].status == CC_SITUATION_RESOLVED);
    CC_CHECK(CcSimRoadCouncil(sim, sim->player.location_id).rows[1].situation_id != sim->situations[slot].id);
    CC_CHECK(CcSimValidate(sim, error, sizeof(error)));
    return 0;
}
