#include "locomotion/cc_humanoid.h"
#include "sim/cc_sim.h"
#include "world/cc_world.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CC_SIMULATION_BUDGET_NS_PER_DAY 50000.0
#define CC_LOCOMOTION_BUDGET_NS_PER_STEP 8000.0

static double ElapsedSeconds(clock_t start)
{
    return (double)(clock() - start) / (double)CLOCKS_PER_SEC;
}

static bool FlatGroundProbe(void *context, CcLimbVec3 origin,
                            float maximum_drop, CcLimbVec3 *point,
                            CcLimbVec3 *normal)
{
    (void)context;
    if (origin.y < 0.0f || origin.y > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static bool ParsePositive(const char *text, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > INT32_MAX) {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}

/* One measurement pass over the seeded worlds. Returns the elapsed seconds
   and the worlds' checksum; a second pass must reproduce the same checksum
   in the same timekeeping regime. */
static double SimulationPass(int32_t sim_seeds, int32_t days_per_seed,
                             uint64_t *checksum,
                             char *error, size_t error_capacity)
{
    uint64_t pass_checksum = 0U;
    CcSim sim;
    clock_t started = clock();
    for (int32_t seed = 0; seed < sim_seeds; ++seed) {
        CcSimInit(&sim, (uint32_t)seed * UINT32_C(0x9e3779b9) + 1U);
        CcSimAdvanceDays(&sim, days_per_seed);
        if (!CcSimValidate(&sim, error, error_capacity)) {
            return -1.0;
        }
        pass_checksum ^= CcSimHash(&sim);
    }
    *checksum = pass_checksum;
    return ElapsedSeconds(started);
}

/* One measurement pass over the locomotion agents. */
static double LocomotionPass(int32_t agent_count, int32_t frames,
                             uint64_t *checksum)
{
    CcHumanoidGait *gaits = calloc((size_t)agent_count, sizeof(*gaits));
    CcLimbVec3 *positions = calloc((size_t)agent_count, sizeof(*positions));
    if (gaits == NULL || positions == NULL) {
        free(gaits);
        free(positions);
        *checksum = UINT64_MAX;
        return -1.0;
    }
    for (int32_t agent = 0; agent < agent_count; ++agent) {
        positions[agent] = (CcLimbVec3){(float)(agent % 8), 0.0f,
                                        (float)(agent / 8)};
        CcHumanoidGaitInit(&gaits[agent], positions[agent], 0.0f,
                           FlatGroundProbe, NULL);
    }
    uint64_t pass_checksum = 0U;
    clock_t started = clock();
    for (int32_t frame = 0; frame < frames; ++frame) {
        for (int32_t agent = 0; agent < agent_count; ++agent) {
            float direction = (agent & 1) != 0 ? -1.0f : 1.0f;
            CcHumanoidGaitAdvance(
                &gaits[agent], positions[agent], 0.0f,
                (CcLimbVec3){0.0f, 0.0f, direction * 1.20f}, true,
                1.0f / 60.0f, FlatGroundProbe, NULL);
            positions[agent].x += gaits[agent].root_velocity.x / 60.0f;
            positions[agent].z += gaits[agent].root_velocity.z / 60.0f;
        }
    }
    double seconds = ElapsedSeconds(started);
    for (int32_t agent = 0; agent < agent_count; ++agent) {
        pass_checksum ^= (uint64_t)(gaits[agent].phase * 1000000.0f);
        pass_checksum ^= (uint64_t)(int64_t)(positions[agent].z * 1000.0f);
    }
    free(gaits);
    free(positions);
    *checksum = pass_checksum;
    return seconds;
}

int main(int argc, char **argv)
{
    int32_t sim_seeds = 100;
    int32_t sim_years = 10;
    int32_t agent_count = 24;
    int32_t locomotion_frames = 3600;
    bool assert_budget = false;
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--quick") == 0) {
            sim_seeds = 8;
            sim_years = 2;
            agent_count = 6;
            locomotion_frames = 600;
        } else if (strcmp(argv[argument], "--assert-budget") == 0) {
            assert_budget = true;
        } else if (argument + 1 < argc &&
                   strcmp(argv[argument], "--sim-seeds") == 0) {
            if (!ParsePositive(argv[++argument], &sim_seeds)) return EXIT_FAILURE;
        } else if (argument + 1 < argc &&
                   strcmp(argv[argument], "--sim-years") == 0) {
            if (!ParsePositive(argv[++argument], &sim_years)) return EXIT_FAILURE;
        } else if (argument + 1 < argc &&
                   strcmp(argv[argument], "--agents") == 0) {
            if (!ParsePositive(argv[++argument], &agent_count)) return EXIT_FAILURE;
        } else if (argument + 1 < argc &&
                   strcmp(argv[argument], "--frames") == 0) {
            if (!ParsePositive(argv[++argument], &locomotion_frames)) {
                return EXIT_FAILURE;
            }
        } else {
            (void)fprintf(stderr, "Unknown benchmark argument: %s\n",
                          argv[argument]);
            return EXIT_FAILURE;
        }
    }
    if (sim_years > INT32_MAX / 365) {
        (void)fprintf(stderr,
                      "Simulation years exceed the supported day range.\n");
        return EXIT_FAILURE;
    }
    int32_t simulation_days_per_seed = sim_years * 365;

    uint64_t checksum = 0U;
    char validation[192];
    /* A shared CI runner can slow a single pass by a few percent while the
       same code runs at its usual rate the next minute. The budget keeps
       the fastest of three passes -- the machine's real capability -- and
       verifies each pass reproduces the same worlds. */
    double sim_seconds = SimulationPass(
        sim_seeds, simulation_days_per_seed, &checksum,
        validation, sizeof(validation));
    if (sim_seconds < 0.0) {
        (void)fprintf(stderr, "%s\n", validation);
        return EXIT_FAILURE;
    }
    if (assert_budget) {
        for (int32_t pass = 1; pass < 3; ++pass) {
            uint64_t again = 0U;
            double seconds = SimulationPass(
                sim_seeds, simulation_days_per_seed, &again,
                validation, sizeof(validation));
            if (seconds < 0.0) {
                (void)fprintf(stderr, "%s\n", validation);
                return EXIT_FAILURE;
            }
            if (again != checksum) {
                (void)fprintf(stderr,
                              "Simulation benchmark diverged across passes.\n");
                return EXIT_FAILURE;
            }
            if (seconds < sim_seconds) sim_seconds = seconds;
        }
    }
    int64_t simulated_days =
        (int64_t)sim_seeds * simulation_days_per_seed;
    double nanoseconds_per_day = sim_seconds * 1.0e9 / (double)simulated_days;

    enum { TERRAIN_SIDE = 64 };
    CcSim sim;
    clock_t started = clock();
    for (int32_t seed = 0; seed < sim_seeds; ++seed) {
        CcWorldManifest manifest;
        CcSimInit(&sim, (uint32_t)seed * UINT32_C(0x9e3779b9) + 1U);
        if (!CcWorldManifestBuild(&manifest, &sim)) return EXIT_FAILURE;
        for (int32_t row = 0; row < TERRAIN_SIDE; ++row) {
            float z = manifest.minimum_z +
                (manifest.maximum_z - manifest.minimum_z) *
                    ((float)row + 0.5f) / (float)TERRAIN_SIDE;
            for (int32_t column = 0; column < TERRAIN_SIDE; ++column) {
                float x = manifest.minimum_x +
                    (manifest.maximum_x - manifest.minimum_x) *
                        ((float)column + 0.5f) / (float)TERRAIN_SIDE;
                float height = CcWorldTerrainHeight(&manifest, x, z);
                checksum = checksum * UINT64_C(1099511628211) ^
                    (uint64_t)(int64_t)(height * 1000.0f);
                checksum ^= (uint64_t)CcWorldSurfaceAt(&manifest, x, z);
            }
        }
    }
    double terrain_seconds = ElapsedSeconds(started);
    int64_t terrain_points = (int64_t)sim_seeds * TERRAIN_SIDE * TERRAIN_SIDE;

    uint64_t locomotion_checksum = 0U;
    double locomotion_seconds = LocomotionPass(
        agent_count, locomotion_frames, &locomotion_checksum);
    if (locomotion_seconds < 0.0) {
        (void)fprintf(stderr, "Could not allocate locomotion benchmark agents.\n");
        return EXIT_FAILURE;
    }
    if (assert_budget) {
        for (int32_t pass = 1; pass < 3; ++pass) {
            uint64_t again = 0U;
            double seconds = LocomotionPass(
                agent_count, locomotion_frames, &again);
            if (seconds < 0.0) {
                (void)fprintf(stderr,
                              "Could not allocate locomotion benchmark agents.\n");
                return EXIT_FAILURE;
            }
            if (again != locomotion_checksum) {
                (void)fprintf(stderr,
                              "Locomotion benchmark diverged across passes.\n");
                return EXIT_FAILURE;
            }
            if (seconds < locomotion_seconds) locomotion_seconds = seconds;
        }
    }
    checksum ^= locomotion_checksum;
    int64_t agent_steps = (int64_t)agent_count * locomotion_frames;
    double nanoseconds_per_step = locomotion_seconds * 1.0e9 /
                                  (double)agent_steps;

    (void)printf("simulation: seeds=%d years=%d days=%" PRId64
                 " cpu=%.6fs ns/day=%.1f\n",
                 sim_seeds, sim_years, simulated_days, sim_seconds,
                 nanoseconds_per_day);
    (void)printf("locomotion: agents=%d frames=%d steps=%" PRId64
                 " cpu=%.6fs ns/step=%.1f\n",
                 agent_count, locomotion_frames, agent_steps,
                 locomotion_seconds, nanoseconds_per_step);
    (void)printf("terrain: seeds=%d points=%" PRId64
                 " cpu=%.6fs ns/point=%.1f\n",
                 sim_seeds, terrain_points, terrain_seconds,
                 terrain_seconds * 1.0e9 / (double)terrain_points);
    (void)printf("checksum=%016" PRIx64 "\n", checksum);
    if (assert_budget &&
        (nanoseconds_per_day > CC_SIMULATION_BUDGET_NS_PER_DAY ||
         nanoseconds_per_step > CC_LOCOMOTION_BUDGET_NS_PER_STEP)) {
        (void)fprintf(stderr,
                      "performance budget exceeded: simulation %.1f/%.0f ns, locomotion %.1f/%.0f ns\n",
                      nanoseconds_per_day,
                      CC_SIMULATION_BUDGET_NS_PER_DAY,
                      nanoseconds_per_step,
                      CC_LOCOMOTION_BUDGET_NS_PER_STEP);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
