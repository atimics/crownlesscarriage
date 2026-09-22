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
        CcLocalCourseResolveContactsInternal(course, player);
        CcLocalCreatureGaitsFixedStepInternal((float)fixed_step);
        course->world_simulation_accumulator -= fixed_step;
    }
    if (course->world_simulation_accumulator < 0.0) {
        course->world_simulation_accumulator = 0.0;
    }
    CcLocalCourseInterpolateInternal(course, (float)(
        course->world_simulation_accumulator / fixed_step));
}

static int32_t CcLocalWorldUpdateInternal(
    CcLocalCourse *course, CcLocalAgent *player, const CcSim *sim,
    float delta_time, bool market_interior, bool advance_course,
    bool step_gaits)
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
            CcLocalCourseResolveContactsInternal(course, player);
        }
        if (step_gaits) {
            CcLocalCreatureGaitsFixedStepInternal((float)fixed_step);
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

int32_t CcLocalWorldUpdate(CcLocalCourse *course, CcLocalAgent *player,
                           const CcSim *sim, float delta_time,
                           bool market_interior, bool advance_course)
{
    return CcLocalWorldUpdateInternal(course, player, sim, delta_time,
                                      market_interior, advance_course, true);
}

/* Step the agent and course but leave the creature gaits to the caller, so a
   travel frame can publish the current carriage target first and then walk the
   rigs toward it. */
int32_t CcLocalWorldUpdateNoGaits(CcLocalCourse *course, CcLocalAgent *player,
                                  const CcSim *sim, float delta_time,
                                  bool market_interior, bool advance_course)
{
    return CcLocalWorldUpdateInternal(course, player, sim, delta_time,
                                      market_interior, advance_course, false);
}

void CcLocalCreatureGaitsAdvanceInternal(int32_t steps)
{
    const float fixed_step = 1.0f / 60.0f;
    for (int32_t step = 0; step < steps; ++step) {
        CcLocalCreatureGaitsFixedStepInternal(fixed_step);
    }
}

float CcLocalCourseAlpha(const CcLocalCourse *course)
{
    if (course == NULL) return 0.0f;
    const double fixed_step = 1.0 / 60.0;
    double amount = course->world_simulation_accumulator / fixed_step;
    if (amount < 0.0) amount = 0.0;
    if (amount > 1.0) amount = 1.0;
    return (float)amount;
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

/* A sample belongs to a LOCAL fixed step, including a step on which no
   journey tick elapsed. Direct/frame-driven producers publish at full blend. */
void CcLocalCarriagePublishPose(CcLocalWorldCarriageState *c,
    Vector3 position, float heading, float travelled, bool advance_sample,
    float alpha)
{
    if (c == NULL || !isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(position.z) || !isfinite(heading) || !isfinite(travelled)) return;
    if (advance_sample && c->presentation_valid) {
        c->previous_tick_position = c->position;
        c->previous_heading_yaw = c->heading_yaw;
        c->previous_travelled = c->travelled;
    } else {
        c->previous_tick_position = position;
        c->previous_heading_yaw = heading;
        c->previous_travelled = travelled;
    }
    c->position = position;
    c->heading_yaw = heading;
    c->travelled = travelled;
    c->presentation_valid = true;
    CcLocalCarriageInterpolate(c, alpha);
}

void CcLocalCarriageInterpolate(CcLocalWorldCarriageState *c, float alpha)
{
    if (c == NULL || !c->presentation_valid) return;
    float a = isfinite(alpha) ? fmaxf(0.0f, fminf(1.0f, alpha)) : 1.0f;
    c->render_position = (Vector3){
        c->previous_tick_position.x + (c->position.x - c->previous_tick_position.x) * a,
        c->previous_tick_position.y + (c->position.y - c->previous_tick_position.y) * a,
        c->previous_tick_position.z + (c->position.z - c->previous_tick_position.z) * a};
    c->render_heading_yaw = c->previous_heading_yaw +
        remainderf(c->heading_yaw - c->previous_heading_yaw, 2.0f * PI) * a;
    c->render_travelled = c->previous_travelled +
        (c->travelled - c->previous_travelled) * a;
}

Vector3 CcLocalCarriageRenderPosition(const CcLocalWorldCarriageState *c)
{
    return c->storybook_travel && c->presentation_valid ?
        c->render_position : c->position;
}

float CcLocalCarriageRenderHeading(const CcLocalWorldCarriageState *c)
{
    return c->storybook_travel && c->presentation_valid ?
        c->render_heading_yaw : c->heading_yaw;
}

float CcLocalCarriageRenderDistance(const CcLocalWorldCarriageState *c)
{
    return c->storybook_travel && c->presentation_valid ?
        c->render_travelled : c->travelled;
}

/* Integrate ds/scale, not total_distance/current_scale: resizing a stopped
   carriage must not spin its wheels. Trapezoidal integration handles smoothly
   changing scale; an explicit route/reversal rebase retains angular phase. */
void CcLocalCarriageRoll(CcLocalWorldCarriageState *c, float scale)
{
    if (c == NULL || !isfinite(scale) || scale <= 0.0f) return;
    float distance = CcLocalCarriageRenderDistance(c);
    float heading = CcLocalCarriageRenderHeading(c);
    if (!isfinite(distance) || !isfinite(heading)) return;
    bool rebase = !c->rolling_valid || c->rolling_route_id != c->route_id ||
        fabsf(remainderf(heading - c->rolling_last_heading, 2.0f * PI)) > PI * 0.5f;
    if (!c->rolling_valid) c->rolling_distance = (double)distance / scale;
    else if (!rebase) {
        c->rolling_distance += (double)(distance - c->rolling_last_travelled) *
            0.5 * (1.0 / c->rolling_last_scale + 1.0 / scale);
    }
    c->rolling_last_travelled = distance;
    c->rolling_last_scale = scale;
    c->rolling_last_heading = heading;
    c->rolling_route_id = c->route_id;
    c->rolling_valid = true;
}
