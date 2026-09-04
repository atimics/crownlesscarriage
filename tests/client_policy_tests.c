#include "client/cc_client_policy.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *preferences_path = "client-policy-test.preferences";
    (void)remove(preferences_path);
    CcClientPreferences preferences;
    char preferences_error[160];
    CC_CHECK(CcClientPreferencesLoad(
        preferences_path, &preferences,
        preferences_error, sizeof(preferences_error)));
    CC_CHECK(!preferences.reduced_motion);
    preferences.reduced_motion = true;
    CC_CHECK(CcClientPreferencesSave(
        preferences_path, &preferences,
        preferences_error, sizeof(preferences_error)));
    preferences.reduced_motion = false;
    CC_CHECK(CcClientPreferencesLoad(
        preferences_path, &preferences,
        preferences_error, sizeof(preferences_error)));
    CC_CHECK(preferences.reduced_motion);
    FILE *invalid_preferences = fopen(preferences_path, "wb");
    CC_CHECK(invalid_preferences != NULL);
    CC_CHECK(fputs("CROWNLESS_PREFERENCES 1\nreduced_motion 7\n",
                   invalid_preferences) >= 0);
    CC_CHECK(fclose(invalid_preferences) == 0);
    CC_CHECK(!CcClientPreferencesLoad(
        preferences_path, &preferences,
        preferences_error, sizeof(preferences_error)));
    CC_CHECK(!preferences.reduced_motion);
    CC_CHECK(strstr(preferences_error, "invalid") != NULL);
    CC_CHECK(!CcClientHitEffectVisible(true, 0.22f));
    CC_CHECK(CcClientHitEffectVisible(false, 0.22f));
    CC_CHECK(!CcClientHitEffectVisible(false, 0.0f));
    CC_CHECK(!CcClientHitEffectVisible(false, NAN));
    (void)remove(preferences_path);

    CcClientDepartureTransition town_exit;
    CcClientDepartureBegin(&town_exit);
    CC_CHECK(town_exit.phase == CC_CLIENT_DEPARTURE_TOWN);
    CC_CHECK(town_exit.town_progress == 0.0f);
    CC_CHECK(town_exit.road_book_progress == 0.0f);
    int departure_updates = 0;
    while (town_exit.phase == CC_CLIENT_DEPARTURE_TOWN &&
           departure_updates < 1000) {
        float previous = town_exit.town_progress;
        CcClientDepartureAdvance(&town_exit, 1.0f, 1.0f / 60.0f);
        CC_CHECK(town_exit.town_progress >= previous);
        departure_updates += 1;
    }
    CC_CHECK(town_exit.phase == CC_CLIENT_DEPARTURE_ROAD_BOOK);
    CC_CHECK(town_exit.town_progress == 1.0f);
    CC_CHECK(town_exit.road_book_progress == 0.0f);
    int camera_updates = 0;
    while (town_exit.phase == CC_CLIENT_DEPARTURE_ROAD_BOOK &&
           camera_updates < 120) {
        float previous = town_exit.road_book_progress;
        CcClientDepartureAdvance(&town_exit, 1.0f, 1.0f / 60.0f);
        CC_CHECK(town_exit.road_book_progress >= previous);
        camera_updates += 1;
    }
    CC_CHECK(town_exit.phase == CC_CLIENT_DEPARTURE_READY);
    CC_CHECK(town_exit.road_book_progress == 1.0f);
    CcClientDepartureAdvance(&town_exit, 0.0f, 10.0f);
    CC_CHECK(town_exit.phase == CC_CLIENT_DEPARTURE_READY);
    CC_CHECK(town_exit.town_progress == 1.0f);
    CC_CHECK(town_exit.road_book_progress == 1.0f);

    CcClientDepartureTransition paused_exit;
    CcClientDepartureBegin(&paused_exit);
    CcClientDepartureAdvance(&paused_exit, 0.0f, 10.0f);
    CC_CHECK(paused_exit.phase == CC_CLIENT_DEPARTURE_TOWN);
    CC_CHECK(paused_exit.town_progress == 0.0f);

    CcClientArrivalTransition arrival;
    CcClientArrivalBegin(&arrival);
    CC_CHECK(arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK);
    CC_CHECK(arrival.road_book_progress == 0.0f);
    CC_CHECK(arrival.town_progress == 0.0f);
    CC_CHECK(CcClientArrivalCameraWeight(&arrival) == 1.0f);
    int arrival_camera_updates = 0;
    while (arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK &&
           arrival_camera_updates < 120) {
        float previous = arrival.road_book_progress;
        CcClientArrivalAdvance(&arrival, 1.0f, 1.0f / 60.0f);
        CC_CHECK(arrival.road_book_progress >= previous);
        CC_CHECK(CcClientArrivalCameraWeight(&arrival) <=
                 1.0f - previous);
        arrival_camera_updates += 1;
    }
    CC_CHECK(arrival.phase == CC_CLIENT_ARRIVAL_TOWN);
    CC_CHECK(arrival.road_book_progress == 1.0f);
    CC_CHECK(arrival.town_progress == 0.0f);
    CC_CHECK(CcClientArrivalCameraWeight(&arrival) == 0.0f);
    CcClientArrivalAdvance(&arrival, 0.0f, 10.0f);
    CC_CHECK(arrival.phase == CC_CLIENT_ARRIVAL_TOWN);
    CC_CHECK(arrival.town_progress == 0.0f);
    int arrival_town_updates = 0;
    while (arrival.phase == CC_CLIENT_ARRIVAL_TOWN &&
           arrival_town_updates < 1000) {
        float previous = arrival.town_progress;
        CcClientArrivalAdvance(&arrival, 1.0f, 1.0f / 60.0f);
        CC_CHECK(arrival.town_progress >= previous);
        arrival_town_updates += 1;
    }
    CC_CHECK(arrival.phase == CC_CLIENT_ARRIVAL_PARKED);
    CC_CHECK(arrival.town_progress == 1.0f);

    CcClientArrivalTransition reduced_motion;
    CcClientArrivalBegin(&reduced_motion);
    CcClientArrivalAdvance(&reduced_motion, 1.0f, NAN);
    CC_CHECK(reduced_motion.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK);
    CC_CHECK(reduced_motion.road_book_progress == 0.0f);
    CcClientArrivalComplete(&reduced_motion);
    CC_CHECK(reduced_motion.phase == arrival.phase);
    CC_CHECK(reduced_motion.road_book_progress == arrival.road_book_progress);
    CC_CHECK(reduced_motion.town_progress == arrival.town_progress);

    float departure = 0.0f;
    for (int frame = 0; frame < 180; ++frame) {
        departure = CcClientConvoyPaceStep(
            departure, false, false, false, false, 1.0f / 60.0f);
    }
    CC_CHECK(departure > 0.99f);

    float road = 0.0f;
    for (int frame = 0; frame < 180; ++frame) {
        road = CcClientConvoyPaceStep(
            road, true, false, false, false, 1.0f / 60.0f);
    }
    CC_CHECK(fabsf(road - 0.72f) < 0.01f);
    CC_CHECK(CcClientConvoyPaceStep(
                 road, true, false, false, true, 1.0f / 60.0f) == 0.0f);
    CC_CHECK(CcClientConvoyPaceStep(
                 0.50f, true, true, false, false, 1.0f) > 0.50f);
    CC_CHECK(CcClientConvoyPaceStep(
                 0.50f, true, false, true, false, 1.0f) < 0.50f);

    float approach = CcClientRoadApproachStep(0.0f, 0.72f, 1.0f);
    CC_CHECK(approach > 0.17f && approach < 0.18f);
    CC_CHECK(CcClientRoadApproachStep(approach, 0.0f, 1.0f) == approach);
    CC_CHECK(CcClientRoadApproachStep(0.95f, 1.0f, 1.0f) == 1.0f);
    CC_CHECK(CcClientConvoyPosturePace(0) == 0.52f);
    CC_CHECK(CcClientConvoyPosturePace(1) == 0.72f);
    CC_CHECK(CcClientConvoyPosturePace(2) == 1.0f);
    CC_CHECK(CcClientStepConvoyPosture(1, -1) == 0);
    CC_CHECK(CcClientStepConvoyPosture(1, 1) == 2);
    CC_CHECK(CcClientStepConvoyPosture(0, -1) == 0);
    CC_CHECK(CcClientStepConvoyPosture(2, 1) == 2);
    CC_CHECK(CcClientConvoyGaitForPace(0.0f) ==
             CC_CLIENT_CONVOY_GAIT_HALT);
    CC_CHECK(CcClientConvoyGaitForPace(NAN) ==
             CC_CLIENT_CONVOY_GAIT_HALT);
    CC_CHECK(CcClientConvoyGaitForPace(0.52f) ==
             CC_CLIENT_CONVOY_GAIT_WALK);
    CC_CHECK(CcClientConvoyGaitForPace(0.72f) ==
             CC_CLIENT_CONVOY_GAIT_TROT);
    CC_CHECK(CcClientConvoyGaitForPace(1.0f) ==
             CC_CLIENT_CONVOY_GAIT_CANTER);
    CC_CHECK(CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_HALT) == 0.0f);
    CC_CHECK(CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_WALK) <
             CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_TROT));
    CC_CHECK(CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_TROT) <
             CcClientConvoyGaitCadence(CC_CLIENT_CONVOY_GAIT_CANTER));
    CC_CHECK(CcClientConvoyGaitName(CC_CLIENT_CONVOY_GAIT_TROT)[0] == 'T');
    CC_CHECK(CcClientRoadHasNextBranch(0, 2));
    CC_CHECK(!CcClientRoadHasNextBranch(1, 2));
    CC_CHECK(!CcClientRoadHasNextBranch(-1, 2));

    CC_CHECK(CcClientMapCommandEnabled(true, true, true, 99.0f));
    CC_CHECK(CcClientMapCommandEnabled(false, false, false, 1.0f));
    CC_CHECK(!CcClientMapCommandEnabled(false, false, false, 2.0f));
    CC_CHECK(!CcClientMapCommandEnabled(false, true, false, 1.0f));
    CC_CHECK(!CcClientMapCommandEnabled(false, false, true, 1.0f));

    CC_CHECK(CcClientPromiseCanBeAccepted(false, 1.0f));
    CC_CHECK(!CcClientPromiseCanBeAccepted(false, 2.0f));
    CC_CHECK(!CcClientPromiseCanBeAccepted(true, 1.0f));

    CC_CHECK(!CcClientInteractionActivated(false, 0.0f, 1.25f));
    CC_CHECK(CcClientInteractionActivated(true, 1.24f, 1.25f));
    CC_CHECK(!CcClientInteractionActivated(true, 1.25f, 1.25f));
    CC_CHECK(!CcClientInteractionActivated(true, NAN, 1.25f));
    CC_CHECK(!CcClientInteractionActivated(true, 0.0f, -1.0f));

    CC_CHECK(CcClientCampaignAccessFor(true, true) ==
             CC_CLIENT_CAMPAIGN_PERSISTENT);
    CC_CHECK(CcClientCampaignAccessFor(true, false) ==
             CC_CLIENT_CAMPAIGN_BLOCKED);
    CC_CHECK(CcClientCampaignAccessFor(false, false) ==
             CC_CLIENT_CAMPAIGN_EPHEMERAL);

    puts("Client policy tests passed");
    return 0;
}
