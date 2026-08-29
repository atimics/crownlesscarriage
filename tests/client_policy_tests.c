#include "client/cc_client_policy.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
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

    puts("Client policy tests passed");
    return 0;
}
