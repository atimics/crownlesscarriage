#include "client/cc_local3d.h"
#include "client/cc_client_policy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static bool Near(float measured, float expected, float tolerance)
{
    return fabsf(measured - expected) <= tolerance;
}

/* Everything the carriage animates comes off the ground it has covered, so
   where the carriage is is all a frame needs to say. The clock and the pace
   are still handed to these two because a frame has them, and the whole point
   of the test is that neither is read. Put back what this change removed --

       static float phase, last_clock;
       float step = fmaxf(0.0f, fminf(0.1f, clock - last_clock));
       phase = fmodf(phase + step * pace * 2.2f / 0.80f, 2.0f * PI);
       last_clock = clock;
       return phase;

   for the wheel, and clock * CcClientConvoyGaitCadence(gait) for the team --
   and the assertions below fail. */
static float WheelAngleForFrame(float clock, float pace,
                                int32_t progress_milli, float route_length,
                                float radius)
{
    (void)clock;
    (void)pace;
    return CcLocalCarriageWheelAngleInternal(
        CcLocalRoadCarriageTravelInternal(progress_milli, route_length), radius);
}

static float GaitPhaseForFrame(float clock, float pace,
                               int32_t progress_milli, float route_length)
{
    (void)clock;
    (void)pace;
    return CcLocalRoadTeamGaitPhaseInternal(
        CcLocalRoadCarriageTravelInternal(progress_milli, route_length));
}

/* One journey down the road book, drawn at whatever frame rate the machine
   manages. A storybook journey swaps the renderer's clock from GetTime() to
   the sim's own tick partway through, so the timeline can be told to hand
   back a clock that jumps backwards. */
static void DriveJourney(int32_t frames, float seconds_per_frame,
                         bool clock_jumps_back, int32_t reached_milli,
                         float route_length, float radius,
                         float *wheel_angle, float *gait_phase)
{
    float clock = 137.0f;
    for (int32_t frame = 0; frame < frames; ++frame) {
        clock += seconds_per_frame;
        if (clock_jumps_back && frame == frames / 2) clock = 4.0f;
        float amount = frames > 1 ? (float)frame / (float)(frames - 1) : 1.0f;
        int32_t progress_milli =
            (int32_t)lroundf(amount * (float)reached_milli);
        *wheel_angle = WheelAngleForFrame(clock, 0.72f, progress_milli,
                                          route_length, radius);
        *gait_phase = GaitPhaseForFrame(clock, 0.72f, progress_milli,
                                        route_length);
    }
}

static void WheelsRollWithTheRoad(void)
{
    float front_model = CcLocalCarriageWheelRadiusInternal(0);
    float rear_model = CcLocalCarriageWheelRadiusInternal(2);
    Require(Near(front_model, 0.81f, 0.0001f) &&
                Near(rear_model, 0.63f, 0.0001f),
            "the model hangs the wheel hubs at the authored radii");

    /* The carriage draws at CARRIAGE_ASSET_SCALE, so the wheel that touches the
       road is smaller than its model radius. Rolling must use the world radius
       or the wheel slips by that scale, and one displayed circumference no
       longer turns it once. */
    float front = CcLocalCarriageWheelWorldRadiusInternal(0);
    float rear = CcLocalCarriageWheelWorldRadiusInternal(2);
    Require(front < front_model && rear < rear_model,
            "the drawn wheel is smaller than its model-space radius");
    for (int32_t wheel = 0; wheel < 4; ++wheel) {
        float world = CcLocalCarriageWheelWorldRadiusInternal(wheel);
        float circumference = 2.0f * PI * world;
        float rolled =
            CcLocalCarriageWheelAngleInternal(circumference, world);
        float back =
            CcLocalCarriageWheelAngleInternal(-circumference, world);
        Require(Near(rolled, 0.0f, 0.01f) ||
                    Near(rolled, 2.0f * PI, 0.01f),
                "one displayed circumference turns the wheel once");
        Require(Near(back, 0.0f, 0.01f) || Near(back, 2.0f * PI, 0.01f),
                "reverse travel turns the wheel back one revolution");
    }

    /* Most of the road book, drawn twice: once at sixty frames a second on a
       rising clock, once at twenty on a clock that jumps backwards partway,
       which is what a storybook journey does to it. The front wheel should
       have turned through the road it covered over its radius, both times. */
    const int32_t reached_milli = 993;
    const float route_length = 374.0f;
    float road = CcLocalRoadCarriageTravelInternal(reached_milli, route_length);
    float expected = fmodf(road / front, 2.0f * PI);
    float smooth = 0.0f;
    float smooth_gait = 0.0f;
    float stuttering = 0.0f;
    float stuttering_gait = 0.0f;
    DriveJourney(3840, 1.0f / 60.0f, false, reached_milli, route_length, front, &smooth,
                 &smooth_gait);
    DriveJourney(320, 1.0f / 20.0f, true, reached_milli, route_length, front, &stuttering,
                 &stuttering_gait);
    printf("front wheel: 60fps %.4f, 20fps with a rewound clock %.4f, "
           "road %.2f / radius %.2f = %.4f\n",
           (double)smooth, (double)stuttering, (double)road, (double)front,
           (double)expected);
    printf("gait at the gate: 60fps %.4f, 20fps with a rewound clock %.4f\n",
           (double)smooth_gait, (double)stuttering_gait);
    Require(Near(smooth, expected, 0.0005f),
            "a journey turns the wheel by its road over its radius");
    Require(Near(smooth, stuttering, 0.0005f),
            "the same road turns the wheel the same way at any frame rate");
    Require(Near(smooth_gait, stuttering_gait, 0.0005f),
            "the same road puts the same foot down at any frame rate");

    /* A carriage that is not moving is not rolling, whatever the clock does
       around it. */
    float halted = WheelAngleForFrame(0.0f, 0.0f, 400, route_length, front);
    float halted_gait = GaitPhaseForFrame(0.0f, 0.0f, 400, route_length);
    for (int32_t frame = 0; frame < 120; ++frame) {
        float clock = (float)frame / 60.0f;
        Require(WheelAngleForFrame(clock, 0.72f, 400, route_length, front) == halted,
                "a standing carriage does not spin its wheels");
        Require(GaitPhaseForFrame(clock, 0.72f, 400, route_length) == halted_gait,
                "a standing team does not walk on the spot");
    }

    /* And ground given back turns the wheel back. */
    float ahead = WheelAngleForFrame(9.0f, 0.72f, 20, route_length, front);
    float behind = WheelAngleForFrame(9.5f, 0.72f, 10, route_length, front);
    float returned = fmodf(ahead - behind + 2.0f * PI, 2.0f * PI);
    Require(Near(returned,
                 CcLocalRoadCarriageTravelInternal(10, route_length) / front,
                 0.0005f),
            "a carriage that gives ground back rolls its wheels back");

    /* Front and rear are one axle apart, so they must roll the same ground.
       Two units is inside a turn of even the small wheel, so nothing here has
       wrapped. */
    const int32_t short_run_milli = 38;
    float short_run = CcLocalRoadCarriageTravelInternal(short_run_milli, route_length);
    float front_angle = 0.0f;
    float rear_angle = 0.0f;
    float ignored = 0.0f;
    DriveJourney(240, 1.0f / 60.0f, false, short_run_milli, route_length, front,
                 &front_angle, &ignored);
    DriveJourney(240, 1.0f / 60.0f, false, short_run_milli, route_length, rear, &rear_angle,
                 &ignored);
    printf("%.3f units of road: front rolls %.4f, rear rolls %.4f\n",
           (double)short_run, (double)(front_angle * front),
           (double)(rear_angle * rear));
    Require(Near(front_angle,
                 CcLocalCarriageWheelAngleInternal(short_run, front),
                 0.0005f) &&
                Near(rear_angle,
                     CcLocalCarriageWheelAngleInternal(short_run, rear),
                     0.0005f),
            "both axles roll the ground the carriage crossed");
}

static void PlaceAndSpinShareOneMeasure(void)
{
    const float route_length = 374.0f;
    for (int32_t progress_milli = 0; progress_milli <= 1000;
         progress_milli += 125) {
        Require(Near(CcLocalRoadCarriageTravelInternal(progress_milli,
                                                        route_length),
                     route_length * (float)progress_milli / 1000.0f,
                     0.0001f),
                "saved progress must map to the sampled route length");
        Require(Near((CcLocalRoadCarriageX(progress_milli) -
                      CcLocalRoadCarriageX(0)) /
                         (CcLocalRoadCarriageX(1000) -
                          CcLocalRoadCarriageX(0)),
                     (float)progress_milli / 1000.0f, 0.0001f),
                "road-book composition must follow progress without measuring ground");
    }
}

static void RealRouteLengthsDriveBothDirections(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x5eed432));
    /* There is no CcWorldStream here. Road-book fallback must be ready from a
       fresh simulation even when open-world setup never ran. */
    Require(sim.route_count >= 2, "the campaign provides multiple real routes");
    for (int32_t index = 0; index < sim.route_count; ++index) {
        const CcRoute *route = &sim.routes[index];
        for (int32_t direction = 0; direction < 2; ++direction) {
            int32_t progress = direction == 0 ? 275 : 725;
            sim.journey.route_id = route->id;
            sim.journey.origin_id = direction == 0 ? route->from_id : route->to_id;
            sim.journey.destination_id = direction == 0 ? route->to_id : route->from_id;
            sim.carriage.progress_milli = progress;
            float length = CcLocalRoadCarriageRouteLengthInternal(&sim);
            Require(isfinite(length) && length > 1.0f,
                    "a real route has a finite physical length");
            float distance = CcLocalRoadCarriageTravelInternal(progress,
                                                                 length);
            float wheel =
                CcLocalCarriageWheelAngleInternal(
                    distance, CcLocalCarriageWheelWorldRadiusInternal(0));
            float gait = CcLocalRoadTeamGaitPhaseInternal(distance);
            Require(isfinite(distance) && distance > 0.0f &&
                        isfinite(wheel) && isfinite(gait),
                    "both route directions keep wheel and gait motion finite");
            Require(Near(distance, length * (float)progress / 1000.0f,
                         0.0001f),
                    "both route directions use saved progress and route length");
        }
    }
    const CcRoute *route = &sim.routes[0];
    CcSettlement *from = CcSimSettlementMutable(&sim, route->from_id);
    sim.journey.route_id = route->id;
    float original = CcLocalRoadCarriageRouteLengthInternal(&sim);
    Require(from != NULL, "the first route has an origin settlement");
    from->map_x += 80;
    float changed = CcLocalRoadCarriageRouteLengthInternal(&sim);
    Require(fabsf(changed - original) > 0.01f,
            "road-book cache refreshes when route geometry changes");
    Require(Near(changed, CcWorldRouteLengthForSim(&sim, route->id),
                 0.0001f),
            "refreshed road-book distance matches a fresh world route");
    from->size = from->size == CC_SETTLEMENT_CAPITAL_SIZE ?
        CC_SETTLEMENT_TOWN : CC_SETTLEMENT_CAPITAL_SIZE;
    float resized = CcLocalRoadCarriageRouteLengthInternal(&sim);
    Require(Near(resized, CcWorldRouteLengthForSim(&sim, route->id),
                 0.0001f),
            "a settlement size change refreshes the warm route-distance cache");
}

static void LegsStepAtTheSpeedTheTeamMoves(void)
{
    float stride = CcLocalRoadTeamStrideInternal();
    Require(stride > 0.0f, "a stride carries the team somewhere");

    /* Equal ground, equal step, wherever along the road it is taken. */
    float step = stride / 8.0f;
    float first = CcLocalRoadTeamGaitPhaseInternal(step) -
                  CcLocalRoadTeamGaitPhaseInternal(0.0f);
    for (int32_t part = 1; part < 7; ++part) {
        float from = step * (float)part;
        float advance = CcLocalRoadTeamGaitPhaseInternal(from + step) -
                        CcLocalRoadTeamGaitPhaseInternal(from);
        Require(Near(advance, first, 0.0005f),
                "the gait advances with the ground and nothing else");
    }
    Require(Near(first, 2.0f * PI / 8.0f, 0.0005f),
            "an eighth of a stride is an eighth of the cycle");
    Require(Near(CcLocalRoadTeamGaitPhaseInternal(4.0f + stride),
                 CcLocalRoadTeamGaitPhaseInternal(4.0f), 0.0005f),
            "a stride puts the same foot down again");

}

int main(void)
{
    WheelsRollWithTheRoad();
    PlaceAndSpinShareOneMeasure();
    RealRouteLengthsDriveBothDirections();
    LegsStepAtTheSpeedTheTeamMoves();
    puts("Distance-driven travel animation passed");
    return 0;
}
