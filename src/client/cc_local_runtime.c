#include "client/cc_local3d_internal.h"

#include <math.h>

static int32_t AccumulateLocalWorldTime(double *accumulator,
                                        float delta_time)
{
    const double fixed_step = 1.0 / 60.0;
    const int32_t maximum_steps = 6;
    double frame_time = fmax(0.0, fmin((double)delta_time, 0.10));
    *accumulator = fmin(*accumulator + frame_time,
                        fixed_step * (double)maximum_steps);
    int32_t steps = (int32_t)floor(
        (*accumulator + 0.000000001) / fixed_step);
    return steps > maximum_steps ? maximum_steps : steps;
}

static void RefreshStreetMarketCrates(const CcSim *sim)
{
    int32_t crates = 0;
    if (sim != NULL) {
        const CcSettlement *place = CcSimSettlement(
            sim, sim->player.location_id);
        if (place != NULL) crates = place->stock[CC_GOOD_FOOD] / 12;
    }
    CcLocalSetStreetMarketCratesInternal(crates);
}

void CcLocalCourseUpdate(CcLocalCourse *course, CcLocalAgent *player,
                         const CcSim *sim, float delta_time)
{
    if (course == NULL) return;
    RefreshStreetMarketCrates(sim);
    const double fixed_step = 1.0 / 60.0;
    int32_t steps = AccumulateLocalWorldTime(
        &course->world_simulation_accumulator, delta_time);
    for (int32_t step = 0; step < steps; ++step) {
        CcLocalCourseFixedStepInternal(course, player, sim,
                                       (float)fixed_step);
        course->world_simulation_accumulator -= fixed_step;
    }
    if (course->world_simulation_accumulator < 0.0) {
        course->world_simulation_accumulator = 0.0;
    }
    CcLocalCourseInterpolateInternal(course, (float)(
        course->world_simulation_accumulator / fixed_step));
}

int32_t CcLocalWorldUpdate(CcLocalCourse *course, CcLocalAgent *player,
                           const CcSim *sim, float delta_time,
                           bool market_interior, bool advance_course)
{
    RefreshStreetMarketCrates(sim);
    if (course == NULL) {
        if (player != NULL) {
            CcLocalAgentUpdate(player, delta_time, market_interior);
        }
        return 0;
    }
    const double fixed_step = 1.0 / 60.0;
    int32_t steps = AccumulateLocalWorldTime(
        &course->world_simulation_accumulator, delta_time);
    for (int32_t step = 0; step < steps; ++step) {
        if (player != NULL) {
            CcLocalAgentFixedStepInternal(player, (float)fixed_step,
                                          market_interior);
        }
        if (advance_course) {
            CcLocalCourseFixedStepInternal(course, player, sim,
                                           (float)fixed_step);
        }
        course->world_simulation_accumulator -= fixed_step;
    }
    if (course->world_simulation_accumulator < 0.0) {
        course->world_simulation_accumulator = 0.0;
    }
    float amount = (float)(course->world_simulation_accumulator / fixed_step);
    if (player != NULL) CcLocalAgentInterpolateInternal(player, amount);
    if (advance_course) CcLocalCourseInterpolateInternal(course, amount);
    return steps;
}

void CcLocalAgentUpdate(CcLocalAgent *agent, float delta_time,
                        bool market_interior)
{
    if (agent == NULL) return;
    const float fixed_step = 1.0f / 60.0f;
    const int32_t maximum_steps = 6;
    float frame_time = fmaxf(0.0f, fminf(delta_time, 0.10f));
    agent->simulation_accumulator = fminf(
        agent->simulation_accumulator + frame_time,
        fixed_step * (float)maximum_steps);
    int32_t steps = 0;
    while (agent->simulation_accumulator + 0.0000001f >= fixed_step &&
           steps < maximum_steps) {
        CcLocalAgentFixedStepInternal(agent, fixed_step, market_interior);
        agent->simulation_accumulator -= fixed_step;
        if (agent->simulation_accumulator < 0.0f) {
            agent->simulation_accumulator = 0.0f;
        }
        steps += 1;
    }
    CcLocalAgentInterpolateInternal(
        agent, agent->simulation_accumulator / fixed_step);
}
