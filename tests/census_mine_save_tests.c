#include "sim/cc_mine.h"
#include "sim/cc_census.h"
#include "persistence/cc_save.h"
#include "test_support.h"

#include <string.h>

/* Audit: a current-schema save must round-trip a world that has BOTH a
   fully populated resident census (schema 117+) AND an active, in-progress
   mine visit (schema 103+) at once. Those two features were added by
   different, independently-developed branches and each has its own save
   table (resident_census, mine_state); this test exists to catch a bug
   where saving or loading one silently drops or corrupts the other, which
   a test that only exercises census OR only exercises the mine would miss.

   It runs the round trip through both persistence paths the game actually
   uses: the sqlite file path (CcSaveWrite/CcSaveRead) and the in-memory
   encode used for network/coop transfer (CcSaveEncode/CcSaveDecode). */

static char error[256];

static bool Apply(CcSim *sim, const CcCommand *command)
{
    if (!CcSimApply(sim, command, error, sizeof(error))) {
        (void)fprintf(stderr, "census/mine setup: %s\n", error);
        return false;
    }
    return true;
}

/* Drive the player to the mine's road-site branch and actually enter it
   through the real command path (CC_COMMAND_TRAVEL then
   CC_COMMAND_VISIT_MINE), the same sequence mine_tests.c's AtBranch/
   ReadyAtHaulers use. CcSimValidate ties an active mine visit to journey
   state (active, TRAVELLING, parked at the mine's branch subtick), so a
   hand-built CcMineVisit without that journey context is invalid -- the
   only realistic way to reach "active mine visit" is to actually visit. */
static bool EnterMineWithCargo(CcSim *sim)
{
    const CcRoadSite *site = CcMineSite(sim);
    if (site == NULL) return false;
    const CcRoute *road = CcSimRoute(sim, site->route_id);
    if (road == NULL) return false;
    sim->player.location_id = road->from_id;
    sim->carriage.location_id = sim->player.location_id;
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = road->to_id};
    if (!Apply(sim, &travel)) return false;
    sim->pony_company.encounter = -1;
    sim->journey.ambush_pending = false;
    sim->journey.elapsed_subticks = CcMineBranchSubtick(sim) - 1;
    sim->carriage.progress_milli = (int32_t)((int64_t)sim->journey.elapsed_subticks *
        1000 / sim->journey.total_subticks);
    CcSimAdvanceRuntimeTicks(sim, 100);
    if (sim->journey.elapsed_subticks != CcMineBranchSubtick(sim)) return false;
    CcCommand visit = {.kind = CC_COMMAND_VISIT_MINE, .target_id = site->id};
    if (!Apply(sim, &visit)) return false;
    sim->player.cargo[CC_GOOD_BREAD] = 4;
    CcCommand pack = {.kind = CC_COMMAND_MINE_PACK,
        .target_id = (CcId)sim->mine.revision, .good = CC_GOOD_BREAD, .amount = 2};
    return Apply(sim, &pack);
}

static void CheckCensusAndMineFields(const CcSim *sim, const CcSim *restored,
                                      CcId newcomer)
{
    CC_CHECK(restored->census.resident_count == sim->census.resident_count);
    CC_CHECK(restored->census.district_count == sim->census.district_count);
    CC_CHECK(CcCensusResidentById(restored, newcomer) != NULL);
    for (int32_t i = 0; i < sim->census.resident_count; ++i) {
        CC_CHECK(restored->census.residents[i].id == sim->census.residents[i].id);
        CC_CHECK(restored->census.residents[i].district_slot ==
                 sim->census.residents[i].district_slot);
        CC_CHECK(restored->census.residents[i].dwelling_slot ==
                 sim->census.residents[i].dwelling_slot);
    }

    CC_CHECK(restored->mine.phase == sim->mine.phase);
    CC_CHECK(restored->mine.x == sim->mine.x && restored->mine.y == sim->mine.y);
    CC_CHECK(restored->mine.light == sim->mine.light);
    CC_CHECK(restored->mine.steps == sim->mine.steps);
    CC_CHECK(restored->mine.seen == sim->mine.seen);
    CC_CHECK(restored->mine.bar_open == sim->mine.bar_open);
    CC_CHECK(restored->mine.surveyed == sim->mine.surveyed);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
        CC_CHECK(restored->mine.pack[good] == sim->mine.pack[good]);
}

int main(void)
{
    const char *path = "census-mine-roundtrip.ccsave";
    CcSim sim, file_restored, encoded_restored;

    CcSimInit(&sim, UINT32_C(0x71a7e5));
    CC_CHECK(sim.schema_version == CC_SIM_SCHEMA_VERSION);

    /* Confirm the census is genuinely populated, not left at defaults. */
    CC_CHECK(sim.census.resident_count > 0);
    CC_CHECK(sim.census.district_count > 0);

    /* Exercise a real census mutation so more than the initial snapshot is
       under test: growing a town issues a fresh resident ID via the same
       path CcSimAdvanceDays uses during play. */
    sim.settlements[0].population += 1;
    CcCensusReconcile(&sim);
    CcId newcomer = sim.census.residents[sim.census.resident_count - 1].id;
    CC_CHECK(CcCensusResidentById(&sim, newcomer) != NULL);

    CC_CHECK(EnterMineWithCargo(&sim));
    CC_CHECK(sim.mine.phase != CC_MINE_NONE);
    CC_CHECK(sim.mine.pack[CC_GOOD_BREAD] > 0);
    if (!CcSimValidate(&sim, error, sizeof(error)))
        (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

    /* Path 1: the sqlite file the desktop game actually saves to disk. */
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &file_restored, error, sizeof(error)));
    CC_CHECK(file_restored.schema_version == CC_SIM_SCHEMA_VERSION);

    /* Path 2: the in-memory encode used for coop/network transfer. Any bug
       specific to one codec (e.g. a table the sqlite path saves but the
       byte encoder forgets, or vice versa) shows up as a mismatch here
       even though the file path above passed. */
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &encoded_restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(encoded_restored.schema_version == CC_SIM_SCHEMA_VERSION);

    /* Field-by-field checks first, so a failure here points straight at
       whichever subsystem's save/load code dropped state, before the
       whole-state hash check below. */
    CheckCensusAndMineFields(&sim, &file_restored, newcomer);
    CheckCensusAndMineFields(&sim, &encoded_restored, newcomer);

    CC_CHECK(CcSimHash(&sim) == CcSimHash(&file_restored));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&encoded_restored));

    /* Advance all three worlds together after the round trip: if the
       loaded census or mine state were subtly wrong (rather than just
       absent), replay would diverge even though the immediate hash
       matched. */
    CcSimAdvanceDays(&sim, 30);
    CcSimAdvanceDays(&file_restored, 30);
    CcSimAdvanceDays(&encoded_restored, 30);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&file_restored));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&encoded_restored));

    (void)remove(path);
    (void)remove("census-mine-roundtrip.ccsave-wal");
    (void)remove("census-mine-roundtrip.ccsave-shm");
    puts("Census and mine state survive current-schema save round trips "
         "(file and in-memory encode) together.");
    return 0;
}
