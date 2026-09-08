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
                                int32_t progress_milli, float radius)
{
    (void)clock;
    (void)pace;
    return CcLocalCarriageWheelAngleInternal(
        CcLocalRoadCarriageTravelInternal(progress_milli), radius);
}

static float GaitPhaseForFrame(float clock, float pace,
                               int32_t progress_milli)
{
    (void)clock;
    (void)pace;
    return CcLocalRoadTeamGaitPhaseInternal(
        CcLocalRoadCarriageTravelInternal(progress_milli));
}

/* One journey down the road book, drawn at whatever frame rate the machine
   manages. A storybook journey swaps the renderer's clock from GetTime() to
   the sim's own tick partway through, so the timeline can be told to hand
   back a clock that jumps backwards. */
static void DriveJourney(int32_t frames, float seconds_per_frame,
                         bool clock_jumps_back, int32_t reached_milli,
                         float radius, float *wheel_angle, float *gait_phase)
{
    float clock = 137.0f;
    for (int32_t frame = 0; frame < frames; ++frame) {
        clock += seconds_per_frame;
        if (clock_jumps_back && frame == frames / 2) clock = 4.0f;
        float amount = frames > 1 ? (float)frame / (float)(frames - 1) : 1.0f;
        int32_t progress_milli =
            (int32_t)lroundf(amount * (float)reached_milli);
        *wheel_angle = WheelAngleForFrame(clock, 0.72f, progress_milli,
                                          radius);
        *gait_phase = GaitPhaseForFrame(clock, 0.72f, progress_milli);
    }
}

static void WheelsRollWithTheRoad(void)
{
    float front = CcLocalCarriageWheelRadiusInternal(0);
    float rear = CcLocalCarriageWheelRadiusInternal(2);
    Require(Near(front, 0.81f, 0.0001f) && Near(rear, 0.63f, 0.0001f),
            "the wheels roll on the radii the model hangs their hubs at");

    /* Most of the road book, drawn twice: once at sixty frames a second on a
       rising clock, once at twenty on a clock that jumps backwards partway,
       which is what a storybook journey does to it. The front wheel should
       have turned through the road it covered over its radius, both times. */
    const int32_t reached_milli = 993;
    float road = CcLocalRoadCarriageTravelInternal(reached_milli);
    float expected = fmodf(road / front, 2.0f * PI);
    float smooth = 0.0f;
    float smooth_gait = 0.0f;
    float stuttering = 0.0f;
    float stuttering_gait = 0.0f;
    DriveJourney(3840, 1.0f / 60.0f, false, reached_milli, front, &smooth,
                 &smooth_gait);
    DriveJourney(320, 1.0f / 20.0f, true, reached_milli, front, &stuttering,
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
    float halted = WheelAngleForFrame(0.0f, 0.0f, 400, front);
    float halted_gait = GaitPhaseForFrame(0.0f, 0.0f, 400);
    for (int32_t frame = 0; frame < 120; ++frame) {
        float clock = (float)frame / 60.0f;
        Require(WheelAngleForFrame(clock, 0.72f, 400, front) == halted,
                "a standing carriage does not spin its wheels");
        Require(GaitPhaseForFrame(clock, 0.72f, 400) == halted_gait,
                "a standing team does not walk on the spot");
    }

    /* And ground given back turns the wheel back. */
    float ahead = WheelAngleForFrame(9.0f, 0.72f, 20, front);
    float behind = WheelAngleForFrame(9.5f, 0.72f, 10, front);
    Require(behind < ahead &&
                Near(ahead - behind,
                     CcLocalRoadCarriageTravelInternal(10) / front, 0.0005f),
            "a carriage that gives ground back rolls its wheels back");

    /* Front and rear are one axle apart, so they must roll the same ground.
       Two units is inside a turn of even the small wheel, so nothing here has
       wrapped. */
    const int32_t short_run_milli = 38;
    float short_run = CcLocalRoadCarriageTravelInternal(short_run_milli);
    float front_angle = 0.0f;
    float rear_angle = 0.0f;
    float ignored = 0.0f;
    DriveJourney(240, 1.0f / 60.0f, false, short_run_milli, front,
                 &front_angle, &ignored);
    DriveJourney(240, 1.0f / 60.0f, false, short_run_milli, rear, &rear_angle,
                 &ignored);
    printf("%.3f units of road: front rolls %.4f, rear rolls %.4f\n",
           (double)short_run, (double)(front_angle * front),
           (double)(rear_angle * rear));
    Require(Near(front_angle * front, short_run, 0.0005f) &&
                Near(rear_angle * rear, short_run, 0.0005f),
            "both axles roll the ground the carriage crossed");
}

static void PlaceAndSpinShareOneMeasure(void)
{
    for (int32_t progress_milli = 0; progress_milli <= 1000;
         progress_milli += 125) {
        Require(Near(CcLocalRoadCarriageX(progress_milli) -
                         CcLocalRoadCarriageX(0),
                     CcLocalRoadCarriageTravelInternal(progress_milli),
                     0.0001f),
                "where the carriage is and how far it has come agree");
    }
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

    /* The stride is a pony's, and it was checked against cruise so that cruise
       still looks like cruise: an ordinary four-watch journey crosses the road
       book's 52 units in 64 seconds, and at that speed the team should step at
       about the walking cadence the old clock-driven look was tuned around. */
    float cruise = CcLocalRoadCarriageTravelInternal(1000) / 64.0f;
    float cadence = cruise / stride * 2.0f * PI;
    float walk = CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_WALK);
    printf("cruise %.4f units a second, cadence %.2f against a tuned walk "
           "of %.2f\n", (double)cruise, (double)cadence, (double)walk);
    Require(cadence > walk * 0.85f && cadence < walk * 1.15f,
            "a walk at cruise keeps the cadence the look was tuned around");
}

int main(void)
{
    WheelsRollWithTheRoad();
    PlaceAndSpinShareOneMeasure();
    LegsStepAtTheSpeedTheTeamMoves();
    puts("Distance-driven travel animation passed");
    return 0;
}
