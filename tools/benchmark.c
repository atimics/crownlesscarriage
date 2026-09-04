#include "locomotion/cc_humanoid.h"
#include "sim/cc_sim.h"

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
    clock_t started = clock();
    for (int32_t seed = 0; seed < sim_seeds; ++seed) {
        CcSim sim;
        CcSimInit(&sim, (uint32_t)seed * UINT32_C(0x9e3779b9) + 1U);
        CcSimAdvanceDays(&sim, simulation_days_per_seed);
        if (!CcSimValidate(&sim, validation, sizeof(validation))) {
            (void)fprintf(stderr, "Simulation benchmark invalid: %s\n", validation);
            return EXIT_FAILURE;
        }
        checksum ^= CcSimHash(&sim);
    }
    double sim_seconds = ElapsedSeconds(started);
    int64_t simulated_days =
        (int64_t)sim_seeds * simulation_days_per_seed;
    double nanoseconds_per_day = sim_seconds * 1.0e9 / (double)simulated_days;

    CcHumanoidGait *gaits = calloc((size_t)agent_count, sizeof(*gaits));
    CcLimbVec3 *positions = calloc((size_t)agent_count, sizeof(*positions));
    if (gaits == NULL || positions == NULL) {
        free(gaits);
        free(positions);
        (void)fprintf(stderr, "Could not allocate locomotion benchmark agents.\n");
        return EXIT_FAILURE;
    }
    for (int32_t agent = 0; agent < agent_count; ++agent) {
        positions[agent] = (CcLimbVec3){(float)(agent % 8), 0.0f,
                                        (float)(agent / 8)};
        CcHumanoidGaitInit(&gaits[agent], positions[agent], 0.0f,
                           FlatGroundProbe, NULL);
    }

    started = clock();
    for (int32_t frame = 0; frame < locomotion_frames; ++frame) {
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
    double locomotion_seconds = ElapsedSeconds(started);
    int64_t agent_steps = (int64_t)agent_count * locomotion_frames;
    double nanoseconds_per_step = locomotion_seconds * 1.0e9 /
                                  (double)agent_steps;
    for (int32_t agent = 0; agent < agent_count; ++agent) {
        checksum ^= (uint64_t)(gaits[agent].phase * 1000000.0f);
        checksum ^= (uint64_t)(int64_t)(positions[agent].z * 1000.0f);
    }
    free(gaits);
    free(positions);

    (void)printf("simulation: seeds=%d years=%d days=%" PRId64
                 " cpu=%.6fs ns/day=%.1f\n",
                 sim_seeds, sim_years, simulated_days, sim_seconds,
                 nanoseconds_per_day);
    (void)printf("locomotion: agents=%d frames=%d steps=%" PRId64
                 " cpu=%.6fs ns/step=%.1f\n",
                 agent_count, locomotion_frames, agent_steps,
                 locomotion_seconds, nanoseconds_per_step);
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
