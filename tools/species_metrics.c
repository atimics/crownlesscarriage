/* Read-only daily species and cult observations. */
#include "sim/cc_sim.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct History {
    CcId last_event;
    uint64_t active_days, hunger_days, equipment_days, tribute_days;
    uint64_t floor_days, divided_days, afterdragon_days, ritual_due_days;
    uint64_t blocked_members, blocked_devotion, blocked_cohesion, blocked_food;
    uint64_t blocked_coins, blocked_relics, blocked_tools, blocked_weapons;
    uint64_t prepared, raids, empty_raids, tribute_events, rallies, recruits;
    uint64_t rumors, offerings, seeds, defenses, saturated_days;
} History;

static bool Positive(const char *text, int32_t max, int32_t *out)
{
    char *end; errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < 1 || value > max) return false;
    *out = (int32_t)value; return true;
}

static void Observe(const CcSim *sim, History *h)
{
    const CcGoblinSociety *g = &sim->goblins;
    bool active = g->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE;
    h->active_days += active;
    h->hunger_days += active && g->raid_motive == CC_GOBLIN_RAID_HUNGER;
    h->equipment_days += active && g->raid_motive == CC_GOBLIN_RAID_EQUIPMENT;
    h->tribute_days += active && g->raid_motive == CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    h->floor_days += g->members == 12;
    h->divided_days += g->cohesion < 25;
    if (sim->dragon.slain) {
        h->afterdragon_days++;
        h->ritual_due_days += sim->dragon_cult.dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_PREPARING &&
            sim->dragon_cult.dragon_seed_days_remaining <= 0;
        CcRitualOfferingPlan plan = CcSimRitualOfferingPlan(sim);
        h->blocked_members += (plan.blocked & CC_RITUAL_MEMBERS) != 0;
        h->blocked_devotion += (plan.blocked & CC_RITUAL_DEVOTION) != 0;
        h->blocked_cohesion += (plan.blocked & CC_RITUAL_COHESION) != 0;
        h->blocked_food += (plan.blocked & CC_RITUAL_FOOD) != 0;
        h->blocked_coins += (plan.blocked & CC_RITUAL_COINS) != 0;
        h->blocked_relics += (plan.blocked & CC_RITUAL_RELICS) != 0;
        h->blocked_tools += (plan.blocked & CC_RITUAL_TOOLS) != 0;
        h->blocked_weapons += (plan.blocked & CC_RITUAL_WEAPONS) != 0;
    }
    int fresh = 0;
    for (int i = 0; i < sim->event_count; ++i) {
        const CcEvent *e = CcSimRecentEvent(sim, i);
        if (e == NULL || e->id <= h->last_event) break;
        fresh++;
        switch (e->kind) {
            case CC_EVENT_GOBLIN_RAID_PREPARED: h->prepared++; break;
            case CC_EVENT_GOBLIN_RAIDED:
                h->raids++;
                h->empty_raids += e->magnitude == 0 && g->carried_treasure_id == 0U;
                break;
            case CC_EVENT_GOBLIN_TRIBUTE_DELIVERED: h->tribute_events++; break;
            case CC_EVENT_GOBLIN_CULT_RALLIED:
                h->rallies++; if (e->magnitude > 0) h->recruits += (uint64_t)e->magnitude; break;
            case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED: h->rumors++; break;
            case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED: h->offerings++; break;
            case CC_EVENT_GOBLIN_DRAGON_SEED: h->seeds++; break;
            case CC_EVENT_GOBLIN_HOARD_DEFENDED: h->defenses++; break;
            default: break;
        }
    }
    h->saturated_days += fresh == CC_MAX_EVENTS;
    const CcEvent *latest = CcSimRecentEvent(sim, 0);
    if (latest != NULL) h->last_event = latest->id;
}

static void Print(const CcSim *s, const History *h, int seed, int year)
{
    int64_t humans = 0, cows = 0, sheep = 0;
    for (int i = 0; i < s->settlement_count; ++i) {
        const CcSettlement *town = &s->settlements[i];
        humans += town->population;
        cows += town->cow_adults + town->cow_calves;
        sheep += town->sheep_adults + town->sheep_lambs;
    }
    const CcGoblinSociety *g = &s->goblins;
    int common = CcSimCommonPonyCount(s);
    CcHungerSnapshot hunger = CcSimHungerSnapshot(s);
    printf("%d,%" PRIu32 ",%d,%d,%u,%u,%016" PRIx64 ",%d,%" PRId64 ",%d,%d,%" PRId64 ",%" PRId64,
        seed, s->world_seed, year, s->current_day, s->schema_version, s->generator_version, CcSimHash(s),
        !s->dragon.slain && s->dragon.life_stage != CC_DRAGON_STAGE_EGG,
        humans, g->members, common + CC_PONY_COUNT, cows, sheep);
    printf(",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        common, CC_PONY_COUNT, s->dragon.egg_count, s->dragon.whelps_dispersed,
        (int)s->dragon.life_stage, s->dragon_cult.devotion, g->cohesion,
        (int)g->tribute_phase, (int)s->dragon_cult.dragon_seed_phase,
        s->dragon_campaign.victories, g->tributes_delivered, hunger.population_weighted);
#define OUT(field) printf(",%" PRIu64, h->field)
    OUT(active_days); OUT(hunger_days); OUT(equipment_days); OUT(tribute_days);
    OUT(floor_days); OUT(divided_days); OUT(afterdragon_days); OUT(ritual_due_days);
    OUT(blocked_members); OUT(blocked_devotion); OUT(blocked_cohesion); OUT(blocked_food);
    OUT(blocked_coins); OUT(blocked_relics); OUT(blocked_tools); OUT(blocked_weapons);
    OUT(prepared); OUT(raids); OUT(empty_raids); OUT(tribute_events); OUT(rallies); OUT(recruits);
    OUT(rumors); OUT(offerings); OUT(seeds); OUT(defenses); OUT(saturated_days);
#undef OUT
    putchar('\n');
}

int main(int argc, char **argv)
{
    int32_t seed = 1, years = 1000;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            if (!Positive(argv[++i], INT32_MAX, &seed)) return 2;
        } else if (!strcmp(argv[i], "--years") && i + 1 < argc) {
            if (!Positive(argv[++i], (INT32_MAX - 1) / 365, &years)) return 2;
        } else { fprintf(stderr, "Usage: %s [--seed ORDINAL] [--years COUNT]\n", argv[0]); return 2; }
    }
    CcSim *sim = malloc(sizeof(*sim));
    if (sim == NULL) return 2;
    CcSimInit(sim, (uint32_t)seed * UINT32_C(0x9e3779b9));
    History history = {0};
    const CcEvent *latest = CcSimRecentEvent(sim, 0);
    if (latest != NULL) history.last_event = latest->id;
    puts("seed_number,world_seed,year,day,schema_version,generator_version,state_hash,dragon,human,goblin,pony,cow,sheep,common_ponies,named_ponies,dragon_eggs,whelps_dispersed,dragon_stage,goblin_devotion,goblin_cohesion,goblin_phase,seed_phase,campaign_victories,tributes_lifetime,weighted_hunger,active_days,hunger_days,equipment_days,tribute_days,floor_days,divided_days,afterdragon_days,ritual_due_days,blocked_members,blocked_devotion,blocked_cohesion,blocked_food,blocked_coins,blocked_relics,blocked_tools,blocked_weapons,prepared,raids,empty_raids,tribute_events,rallies,recruits,rumors,offerings,seeds,defenses,saturated_days");
    char error[256];
    for (int year = 0; year <= years; ++year) {
        if (year > 0) for (int day = 0; day < 365; ++day) {
            CcSimAdvanceDays(sim, 1); Observe(sim, &history);
        }
        if (!CcSimValidate(sim, error, sizeof(error))) {
            fprintf(stderr, "Seed %d failed in year %d: %s\n", seed, year, error);
            free(sim); return 1;
        }
        if (year <= 10 || year % 25 == 0 || year == years) Print(sim, &history, seed, year);
    }
    free(sim);
    return ferror(stdout) ? 1 : 0;
}
