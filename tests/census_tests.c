#include "persistence/cc_save.h"
#include "client/cc_local_place.h"
#include "sim/cc_census.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <string.h>

static CcSim sim, restored;

static void CheckCensus(const CcSim *world)
{
    char error[256];
    if (!CcSimValidate(world, error, sizeof(error)))
        (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(world, error, sizeof(error)));
    int32_t total = 0;
    for (int32_t town = 0; town < world->settlement_count; ++town) {
        const CcSettlement *place = &world->settlements[town];
        int32_t residents = CcCensusPopulation(world, place->id);
        CC_CHECK(residents == place->population);
        total += residents;
        int32_t across_districts = 0;
        for (int32_t district = town * CC_CENSUS_DISTRICTS_PER_TOWN;
             district < (town + 1) * CC_CENSUS_DISTRICTS_PER_TOWN;
             ++district)
            across_districts += CcCensusDistrictPopulation(
                world, world->census.districts[district].id);
        CC_CHECK(across_districts == residents);
    }
    int32_t living = 0;
    for (int32_t i = 0; i < world->census.resident_count; ++i)
        if (world->census.residents[i].left_day == 0 &&
            !world->census.residents[i].sheltered) ++living;
    CC_CHECK(total == living);
}

int main(void)
{
    const char *path = "resident-census.ccsave";
    char error[256];
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CC_CHECK(sim.census.resident_count == 12323);
    CC_CHECK(sim.census.district_count == 36);
    const int32_t thornford[6] = {64, 240, 240, 440, 392, 87};
    const int32_t homes[6] = {16, 60, 60, 110, 98, 24};
    for (int32_t district = 0; district < 6; ++district) {
        CC_CHECK(CcCensusDistrictPopulation(&sim,
            sim.census.districts[district].id) == thornford[district]);
        CC_CHECK(sim.census.districts[district].dwelling_count == homes[district]);
    }
    char status[160];
    CcLocalTownStatus(&sim, sim.settlements[0].id, status, sizeof(status));
    CC_CHECK(strstr(status, "Market Centre: 64 people / 16 homes") != NULL);
    CC_CHECK(strstr(status,
        "1463 residents across 6 districts / 368 homes") != NULL);
    CheckCensus(&sim);

    CcId known = sim.characters[0].id;
    const CcCensusResident *known_home = CcCensusResidentById(&sim, known);
    CC_CHECK(known_home != NULL && known_home->rich_identity == 1);
    CC_CHECK(known_home->birth_day == sim.characters[0].birth_day);
    CcId future_name = 0U;
    for (int32_t i = 0; i < sim.census.resident_count; ++i) {
        const CcCensusResident *person = &sim.census.residents[i];
        if (sim.census.districts[person->district_slot].settlement_id ==
                sim.settlements[0].id && person->rich_identity == 0) {
            future_name = person->id;
            break;
        }
    }
    CC_CHECK(future_name != 0U);
    sim.player.location_id = sim.settlements[0].id;
    sim.carriage.location_id = sim.player.location_id;
    CcSimPeopleEnterSettlement(&sim);
    CC_CHECK(CcSimCharacter(&sim, future_name) != NULL);
    CC_CHECK(CcSimCharacter(&sim, future_name)->birth_day ==
             CcCensusResidentById(&sim, future_name)->birth_day);
    CheckCensus(&sim);

    /* Save the expanded cast, then follow the same people through a month. */
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcCensusResidentById(&restored, known) != NULL);
    CC_CHECK(CcCensusResidentById(&restored, future_name) != NULL);
    CcSimAdvanceDays(&sim, 30);
    CcSimAdvanceDays(&restored, 30);
    CheckCensus(&sim);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));

    /* Growth issues a new ID. Departure frees the address and spends the ID. */
    sim.settlements[0].population += 1;
    restored.settlements[0].population += 1;
    CcCensusReconcile(&sim);
    CcCensusReconcile(&restored);
    CcId newcomer = sim.census.residents[sim.census.resident_count - 1].id;
    CC_CHECK(CcCensusResidentById(&sim, newcomer) != NULL);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    sim.settlements[0].population -= 2;
    restored.settlements[0].population -= 2;
    CcCensusReconcile(&sim);
    CcCensusReconcile(&restored);
    CC_CHECK(CcCensusResidentById(&sim, newcomer) == NULL);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CheckCensus(&sim);
    sim.settlements[0].population += 1;
    restored.settlements[0].population += 1;
    CcCensusReconcile(&sim);
    CcCensusReconcile(&restored);
    CC_CHECK(sim.census.residents[sim.census.resident_count - 1].id > newcomer);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));

    /* A valid change of address survives direct save and reload. */
    CcId moved = 0U;
    for (int32_t i = 0; i < sim.census.resident_count; ++i) {
        CcCensusResident *person = &sim.census.residents[i];
        if (person->left_day == 0 && person->district_slot == 5 &&
            person->dwelling_slot == 21) {
            moved = person->id;
            person->dwelling_slot = 22;
            break;
        }
    }
    CC_CHECK(moved != 0U);
    CheckCensus(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcCensusResidentById(&restored, moved)->dwelling_slot == 22);

    /* A saved address and a life date each contribute to the replay hash. */
    uint64_t before = CcSimHash(&sim);
    sim.census.residents[100].birth_day -= 1;
    CC_CHECK(CcSimHash(&sim) != before);
    sim.census.residents[100].birth_day += 1;
    CC_CHECK(CcSimHash(&sim) == before);
    sim.census.districts[0].name[0] = 'X';
    CC_CHECK(CcSimHash(&sim) != before);
    sim.census.districts[0].name[0] = 'M';
    CC_CHECK(CcSimHash(&sim) == before);

    /* The previous save schema gains homes while retaining named identities. */
    sim.schema_version = 113U;
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimCharacter(&restored, known) != NULL);
    CC_CHECK(CcCensusResidentById(&restored, known) != NULL);
    CheckCensus(&restored);

    /* A lost town keeps its named people in recorded shelter. */
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    sim.settlements[0].population = 0;
    sim.settlements[0].service_mask = 0U;
    sim.settlements[0].service_project = CC_SERVICE_NONE;
    sim.settlements[0].service_project_days = 0;
    sim.settlements[0].security = 0;
    sim.settlements[0].prosperity = 0;
    CcCensusReconcile(&sim);
    int32_t sheltered = 0;
    for (int32_t i = 0; i < sim.census.resident_count; ++i) {
        const CcCensusResident *person = &sim.census.residents[i];
        if (person->sheltered &&
            sim.census.districts[person->district_slot].settlement_id ==
                sim.settlements[0].id) ++sheltered;
    }
    CC_CHECK(sheltered > 0);
    CheckCensus(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));

    (void)remove(path);
    (void)remove("resident-census.ccsave-wal");
    (void)remove("resident-census.ccsave-shm");
    return 0;
}
