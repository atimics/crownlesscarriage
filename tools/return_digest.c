/* Show The Return's change digest without a window.

   crownless_return_digest [--seed N] [--from TOWN] [--to TOWN] [--days N]

   A new world starts with the company in FROM. The company rides to TO by the
   real roads, waits there N days, and rides back. Each arrival prints the
   digest for that town: what changed since the company last left it. */

#include "sim/cc_return.h"
#include "sim/cc_return_ride.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char error[256];

static bool Ride(CcSim *sim, CcId destination)
{
    return CcReturnRideAlongPath(sim, destination, error, sizeof(error));
}

static CcId TownByName(const CcSim *sim, const char *name)
{
    return CcReturnRideTownByName(sim, name);
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
    int32_t legs = CcReturnRideRoadPath(sim, sim->player.location_id, to, path);
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
