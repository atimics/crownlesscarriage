/* Show The Return's change digest without a window.

   crownless_return_digest [--seed N] [--from TOWN] [--to TOWN] [--days N]

   A new world starts with the company in FROM. The company rides to TO by the
   real roads, waits there N days, and rides back. Each arrival prints the
   digest for that town: what changed since the company last left it. */

#include "sim/cc_return.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char error[256];

static bool Apply(CcSim *sim, CcCommandKind kind, CcId target)
{
    CcCommand command = {.kind = kind, .target_id = target};
    return CcSimApply(sim, &command, error, sizeof(error));
}

static bool ContinuePause(CcSim *sim)
{
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING)
        return Apply(sim, CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                     CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP, 0U);
    if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
        const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
        if (site != NULL) return Apply(sim, CC_COMMAND_PASS_ROAD_SITE, site->id);
        CcRoadLegPreview previews[3];
        int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
        for (int32_t i = 0; i < count; ++i) {
            if (previews[i].direction == sim->journey.road_direction &&
                previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID)
                return Apply(sim, CC_COMMAND_CHOOSE_ROAD_LEG,
                             previews[i].decision_token);
        }
        return false;
    }
    if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED)
        return Apply(sim, CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE, 0U) ||
            Apply(sim, CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT, 0U);
    return false;
}

static bool Ride(CcSim *sim, CcId destination)
{
    if (!Apply(sim, CC_COMMAND_TRAVEL, destination)) return false;
    /* The digest is the subject here, not the road: skip road fights. */
    sim->journey.ambush_pending = false;
    sim->pony_company.encounter = -1;
    for (int32_t step = 0; step < 200000 && sim->journey.active; ++step) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        else if (!ContinuePause(sim))
            return false;
    }
    return !sim->journey.active && sim->player.location_id == destination;
}

static CcId TownByName(const CcSim *sim, const char *name)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (strcmp(sim->settlements[i].name, name) == 0) return sim->settlements[i].id;
    return 0U;
}

/* The shortest road path by leg count; ties go to the lower route slot. */
static int32_t RoadPath(const CcSim *sim, CcId from, CcId to, CcId *path)
{
    CcId previous[CC_MAX_SETTLEMENTS] = {0};
    CcId queue[CC_MAX_SETTLEMENTS];
    bool seen[CC_MAX_SETTLEMENTS] = {false};
    int32_t head = 0, tail = 0;
    queue[tail++] = from;
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == from) seen[i] = true;
    while (head < tail) {
        CcId at = queue[head++];
        for (int32_t r = 0; r < sim->route_count; ++r) {
            const CcRoute *route = &sim->routes[r];
            CcId next = route->from_id == at ? route->to_id :
                route->to_id == at ? route->from_id : 0U;
            for (int32_t i = 0; next != 0U && i < sim->settlement_count; ++i) {
                if (sim->settlements[i].id != next || seen[i]) continue;
                seen[i] = true;
                previous[i] = at;
                queue[tail++] = next;
            }
        }
    }
    int32_t count = 0;
    CcId reversed[CC_MAX_SETTLEMENTS];
    for (CcId at = to; at != from && count < CC_MAX_SETTLEMENTS;) {
        reversed[count++] = at;
        CcId back = 0U;
        for (int32_t i = 0; i < sim->settlement_count; ++i)
            if (sim->settlements[i].id == at) back = previous[i];
        if (back == 0U) return 0;
        at = back;
    }
    for (int32_t i = 0; i < count; ++i) path[i] = reversed[count - 1 - i];
    return count;
}

static void PrintDigest(const CcSim *sim, CcId town)
{
    CcReturnDigest digest;
    char text[8192];
    if (!CcReturnDigestBuild(sim, town, &digest)) return;
    (void)CcReturnDigestText(sim, &digest, text, sizeof(text));
    (void)fputs(text, stdout);
}

static bool RideAll(CcSim *sim, CcId to)
{
    CcId path[CC_MAX_SETTLEMENTS];
    int32_t legs = RoadPath(sim, sim->player.location_id, to, path);
    if (legs == 0) return false;
    for (int32_t i = 0; i < legs; ++i) {
        if (!Ride(sim, path[i])) {
            (void)fprintf(stderr, "The ride to %s stopped: %s\n",
                          CcSimSettlement(sim, path[i])->name, error);
            return false;
        }
        (void)printf("\n== Arrive ");
        PrintDigest(sim, path[i]);
    }
    return true;
}

int main(int argc, char **argv)
{
    uint32_t seed = 1U;
    int32_t days = 60;
    const char *from = "Thornford";
    const char *to = "Silverwick";
    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--seed") == 0) seed = (uint32_t)strtoul(argv[i + 1], NULL, 10);
        else if (strcmp(argv[i], "--days") == 0) days = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "--from") == 0) from = argv[i + 1];
        else if (strcmp(argv[i], "--to") == 0) to = argv[i + 1];
        else {
            (void)fprintf(stderr, "usage: %s [--seed N] [--from TOWN] [--to TOWN] [--days N]\n", argv[0]);
            return 2;
        }
    }
    if (days < 0 || days > 3650) {
        (void)fprintf(stderr, "days must be from 0 to 3650.\n");
        return 2;
    }
    CcSim *sim = calloc(1U, sizeof(*sim));
    if (sim == NULL) return 1;
    CcSimInit(sim, seed);
    CcId home = TownByName(sim, from), away = TownByName(sim, to);
    if (home == 0U || away == 0U || home == away) {
        (void)fprintf(stderr, "unknown or equal towns: %s, %s\n", from, to);
        free(sim);
        return 2;
    }
    if (sim->player.location_id != home) {
        sim->player.location_id = home;
        sim->carriage.location_id = home;
    }
    (void)printf("seed %u, schema %u: the company leaves %s for %s, waits %d days, and returns.\n",
                 seed, sim->schema_version, from, to, days);
    bool ok = RideAll(sim, away);
    if (ok) {
        CcSimAdvanceDays(sim, days);
        (void)printf("\n-- %d days pass in %s --\n", days, to);
        ok = RideAll(sim, home);
    }
    (void)printf("\nstate hash %016llx\n", (unsigned long long)CcSimHash(sim));
    free(sim);
    return ok ? 0 : 1;
}
