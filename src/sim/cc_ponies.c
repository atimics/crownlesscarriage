#include "sim/cc_sim.h"

#include <stdio.h>

static const char *const NAMES[CC_PONY_COUNT] = {
    "Ember", "Marmalade", "Dandelion", "Clover", "Puddle", "Ink", "Velvet"
};
static const char *const PERSONALITIES[CC_PONY_COUNT] = {
    "Brave and impulsive", "Friendly and always hungry", "Cheerfully distracted",
    "Gentle and stubborn", "Playful rain-lover", "Quiet and curious", "Dramatic racer"
};

const char *CcPonyName(int32_t pony)
{
    return pony >= 0 && pony < CC_PONY_COUNT ? NAMES[pony] : "Pony";
}

const char *CcPonyPersonality(int32_t pony)
{
    return pony >= 0 && pony < CC_PONY_COUNT ? PERSONALITIES[pony] : "Traveller";
}

static uint32_t PonyRandom(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return *seed;
}

CcGood CcPonyQuestGood(const CcPony *pony)
{
    static const CcGood goods[] = {CC_GOOD_FOOD, CC_GOOD_WOOL, CC_GOOD_TOOLS};
    return goods[pony->quest_kind % 3];
}

void CcPonyQuestText(const CcSim *sim, int32_t index, char *text, size_t capacity)
{
    static const char *const motives[CC_PONY_COUNT][3] = {
        {"Ember is feeding a stranded patrol.", "Ember wants a brave red banner.", "Ember is fixing a broken warning bell."},
        {"Marmalade found a hungry roadside baker.", "Marmalade wants a picnic blanket.", "Marmalade broke the baker's oven handle."},
        {"Dandelion forgot the picnic basket.", "Dandelion is making trail ribbons.", "Dandelion wants to repair a crooked sign."},
        {"Clover is feeding tired travellers.", "Clover needs blankets for a shelter.", "Clover is repairing the shelter gate."},
        {"Puddle is sharing lunch with a ferryman.", "Puddle needs towels after a swim.", "Puddle is helping mend a ferry rope."},
        {"Ink packed books instead of lunch.", "Ink wants a soft wrap for an old book.", "Ink found a locked roadside chest."},
        {"Velvet is hosting a victory picnic.", "Velvet demands a splendid race ribbon.", "Velvet is repairing the starting gate."}
    };
    if (index < 0 || index >= CC_PONY_COUNT || text == NULL || capacity == 0U) return;
    const CcPony *pony = &sim->pony_company.ponies[index];
    (void)snprintf(text, capacity, "%s Bring %d %s.", motives[index][pony->quest_kind],
                   pony->quest_amount, CcGoodDefinitionFor(CcPonyQuestGood(pony))->name);
}

void CcPoniesInit(CcSim *sim)
{
    CcPonyCompany *company = &sim->pony_company;
    *company = (CcPonyCompany){.encounter = -1};
    uint32_t seed = sim->world_seed ^ UINT32_C(0x7261696e);
    company->team[0] = (int32_t)(PonyRandom(&seed) % CC_PONY_COUNT);
    company->team[1] = (company->team[0] + 1 + (int32_t)(PonyRandom(&seed) % 6U)) % CC_PONY_COUNT;
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        CcPony *pony = &company->ponies[i];
        pony->route_id = sim->routes[PonyRandom(&seed) % (uint32_t)sim->route_count].id;
        pony->quest_kind = (int32_t)(PonyRandom(&seed) % 3U);
        pony->quest_amount = 1 + (int32_t)(PonyRandom(&seed) % 3U);
        pony->health = 100;
        if (i == company->team[0] || i == company->team[1]) {
            pony->route_id = 0U;
            pony->seen = true;
            pony->bond = 1;
        }
    }

}

bool CcPoniesValidate(const CcSim *sim)
{
    const CcPonyCompany *c = &sim->pony_company;
    if (c->team[0] < 0 || c->team[0] >= CC_PONY_COUNT || c->team[1] < 0 ||
        c->team[1] >= CC_PONY_COUNT || c->team[0] == c->team[1] ||
        c->encounter < -1 || c->encounter >= CC_PONY_COUNT) return false;
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        const CcPony *p = &c->ponies[i];
        bool team = i == c->team[0] || i == c->team[1];
        bool route = false;
        bool seen_route = p->last_seen_route == 0U;
        for (int32_t r = 0; r < sim->route_count; ++r) {
            if (p->route_id == sim->routes[r].id) route = true;
            if (p->last_seen_route == sim->routes[r].id) seen_route = true;
        }
        if ((team ? p->route_id != 0U || !p->seen : !route) || !seen_route ||
            p->bond < 0 || p->bond > 1000000 || p->quests_completed < 0 ||
            p->quests_completed > 1000000 || p->releases < 0 || p->releases > 1000000 ||
            p->last_met_day < 0 || p->last_met_day > sim->current_day ||
            p->quest_kind < 0 || p->quest_kind > 2 || p->quest_amount < 1 || p->quest_amount > 3 ||
            p->health < 0 || p->health > 100 || p->fatigue < 0 || p->fatigue > 100 ||
            p->hunger < 0 || p->hunger > 100 || (p->ready && (!p->seen || team))) return false;
    }
    if (c->encounter >= 0 && (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
        c->encounter == c->team[0] || c->encounter == c->team[1] ||
        !c->ponies[c->encounter].seen ||
        c->ponies[c->encounter].route_id != sim->journey.route_id)) return false;
    return true;
}

int32_t CcPonyOnRoad(const CcSim *sim)
{
    if (sim->schema_version < 40U || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) return -1;
    if (sim->pony_company.encounter >= 0) return sim->pony_company.encounter;
    int64_t progress = (int64_t)sim->journey.elapsed_subticks * 100;
    if (progress < (int64_t)sim->journey.total_subticks * 15 ||
        progress > (int64_t)sim->journey.total_subticks * 70) return -1;
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        const CcPony *pony = &sim->pony_company.ponies[i];
        if (pony->route_id == sim->journey.route_id && pony->last_met_day != sim->current_day) return i;
    }
    return -1;
}

bool CcPoniesApply(CcSim *sim, const CcCommand *cmd, char *error, size_t capacity)
{
    CcPonyCompany *c = &sim->pony_company;
    int32_t index = cmd->target_id > 0U && cmd->target_id <= CC_PONY_COUNT ?
        (int32_t)cmd->target_id - 1 : -1;
    const char *failure = "Meet this pony on the road first.";
    if (sim->schema_version < 40U || index < 0) goto fail;
    CcPony *p = &c->ponies[index];
    if (cmd->kind == CC_COMMAND_MEET_PONY) {
        if (c->encounter >= 0 || CcPonyOnRoad(sim) != index) goto fail;
        c->encounter = index;
        p->seen = true;
        p->last_seen_route = p->route_id;
        p->last_met_day = sim->current_day;
        return true;
    }
    if (c->encounter != index) goto fail;
    if (cmd->kind == CC_COMMAND_LEAVE_PONY) {
        c->encounter = -1;
        return true;
    }
    if (cmd->kind == CC_COMMAND_HELP_PONY) {
        failure = "Bring the requested supplies to help this pony.";
        CcGood good = CcPonyQuestGood(p);
        if (p->ready || sim->player.cargo[good] < p->quest_amount || p->bond >= 1000000 ||
            p->quests_completed >= 1000000) goto fail;
        sim->player.cargo[good] -= p->quest_amount;
        p->quests_completed++;
        p->bond++;
        p->ready = true;
        return true;
    }
    if (cmd->kind == CC_COMMAND_SWAP_PONY) {
        failure = "Help the pony, then choose one of your two companions.";
        if (!p->ready || cmd->amount < 0 || cmd->amount > 1) goto fail;
        CcPony *released = &c->ponies[c->team[cmd->amount]];
        if (released->releases >= 1000000 || sim->route_count < 2) goto fail;
        int32_t road = 0;
        for (int32_t r = 0; r < sim->route_count; ++r) {
            if (sim->routes[r].id == sim->journey.route_id) road = r;
        }
        uint32_t seed = sim->world_seed ^ (uint32_t)c->team[cmd->amount] ^ (uint32_t)released->releases;
        road = (road + 1 + (int32_t)(PonyRandom(&seed) % (uint32_t)(sim->route_count - 1))) % sim->route_count;
        released->route_id = sim->routes[road].id;
        released->last_seen_route = sim->journey.route_id;
        released->last_met_day = sim->current_day;
        released->releases++;
        released->quest_kind = (released->quest_kind + 1 + (int32_t)(PonyRandom(&seed) % 2U)) % 3;
        released->quest_amount = 1 + (int32_t)(PonyRandom(&seed) % 3U);
        released->ready = false;
        CcHorse *horse = &sim->horse_team[cmd->amount];
        released->health = horse->health;
        released->fatigue = horse->fatigue;
        released->hunger = horse->hunger;
        horse->health = p->health;
        horse->fatigue = p->fatigue;
        horse->hunger = p->hunger;
        c->team[cmd->amount] = index;
        p->route_id = 0U;
        p->ready = false;
        c->encounter = -1;
        return true;
    }
fail:
    if (error != NULL && capacity > 0U) (void)snprintf(error, capacity, "%s", failure);
    return false;
}
