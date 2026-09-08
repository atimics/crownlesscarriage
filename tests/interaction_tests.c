#include "client/cc_interaction.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "line %d: %s\n", __LINE__, #test); return 1; } } while (0)
int main(void)
{
    /* Road signs share an anchor but keep separate pointer regions. */
    CcInteractionLabelRect viewport = {12, 88, 1016, 420};
    CcInteractionLabelRect labels[CC_INTERACTION_CAPACITY] = {0};
    for (int32_t i = 0; i < 8; ++i) {
        CHECK(CcInteractionPlaceLabel((CcInteractionLabelRect){200, 280, 220, 36},
            viewport, labels, i, &labels[i]));
        CHECK(labels[i].x >= viewport.x && labels[i].y >= viewport.y);
        CHECK(labels[i].x + labels[i].width <= viewport.x + viewport.width);
        CHECK(labels[i].y + labels[i].height <= viewport.y + viewport.height);
        for (int32_t j = 0; j < i; ++j) {
            CHECK(labels[i].y >= labels[j].y + labels[j].height + 4 ||
                  labels[j].y >= labels[i].y + labels[i].height + 4);
        }
    }
    CcInteractionLabelRect edge;
    CHECK(CcInteractionPlaceLabel((CcInteractionLabelRect){-100, -100, 400, 36},
        viewport, NULL, 0, &edge));
    CHECK(edge.x == viewport.x && edge.y == viewport.y);
    CHECK(CcInteractionPlaceLabel((CcInteractionLabelRect){2000, 2000, 400, 36},
        viewport, NULL, 0, &edge));
    CHECK(edge.x + edge.width == viewport.x + viewport.width);
    CHECK(edge.y + edge.height == viewport.y + viewport.height);
    /* A full scene falls back to the corresponding action card. */
    CcInteractionLabelRect full = {12, 88, 1016, 420};
    CHECK(!CcInteractionPlaceLabel((CcInteractionLabelRect){200, 280, 220, 36},
        viewport, &full, 1, &edge));
    CHECK(edge.width == 0);
    CHECK(!CcInteractionPlaceLabel((CcInteractionLabelRect){200, 280, 2000, 36},
        viewport, NULL, 0, &edge));
    CHECK(!CcInteractionPlaceLabel((CcInteractionLabelRect){NAN, 280, 220, 36},
        viewport, NULL, 0, &edge));
    CcInteractionPlan plan = {.count = 2, .targets = {
        {.key = {1, 20, CC_INTERACTION_PERSON}, .visible = true, .available = true,
         .left = 10, .right = 40, .top = 10, .bottom = 80, .camera_distance = 8,
         .approach_x = 10, .approach_z = 10, .radius = 1.8f},
        {.key = {1, 21, CC_INTERACTION_DOOR}, .visible = true, .available = true,
         .left = 10, .right = 40, .top = 10, .bottom = 80, .camera_distance = 12,
         .approach_x = 20, .approach_z = 10, .radius = 1.0f}}};
    CcInteractionState state = {0};
    CHECK(CcInteractionPick(&plan, 20, 20) == &plan.targets[0]);
    CHECK(CcInteractionPick(&plan, NAN, 20) == NULL);
    plan.targets[0].visible = false;
    CHECK(CcInteractionPick(&plan, 20, 20) == &plan.targets[1]);
    CcInteractionCycle(&state, &plan, 1);
    CHECK(CcInteractionKeyEqual(state.focus, plan.targets[1].key));
    plan.targets[0].visible = true;
    CHECK(CcInteractionStart(&state, &plan.targets[0], 0, 0));
    /* Reordering and movement preserve the chosen person. */
    CcInteractionTarget swap = plan.targets[0];
    plan.targets[0] = plan.targets[1]; plan.targets[1] = swap;
    plan.targets[1].approach_x = 12;
    CHECK(!CcInteractionAdvance(&state, &plan, 9, 10, 0.1f));
    CHECK(CcInteractionAdvance(&state, &plan, 12, 10, 0.1f));
    CHECK(!CcInteractionAdvance(&state, &plan, 12, 10, 0.1f));
    CHECK(CcInteractionStart(&state, &plan.targets[1], 0, 0));
    plan.targets[1].available = false;
    (void)snprintf(plan.targets[1].reason, sizeof(plan.targets[1].reason), "Find safety first.");
    CHECK(!CcInteractionAdvance(&state, &plan, 12, 10, 0.1f));
    CHECK(!state.approaching && strcmp(state.feedback, "Find safety first.") == 0);
    CHECK(!CcInteractionStart(&state, &plan.targets[1], 0, 0));
    plan.targets[1].available = true;
    CHECK(CcInteractionStart(&state, &plan.targets[1], 0, 0));
    plan.targets[1].key.place = 2;
    CHECK(!CcInteractionAdvance(&state, &plan, 12, 10, 0.1f));
    CHECK(!state.approaching);
    CHECK(CcInteractionStart(&state, &plan.targets[1], 0, 0));
    for (int i = 0; i < 42; ++i) CHECK(!CcInteractionAdvance(&state, &plan, 0, 0, 0.1f));
    CHECK(!state.approaching && state.feedback[0] != '\0');
    CHECK(CcInteractionStart(&state, &plan.targets[1], 0, 0));
    CHECK(CcInteractionStart(&state, &plan.targets[0], 0, 0));
    CHECK(CcInteractionKeyEqual(state.pending, plan.targets[0].key));
    CcInteractionCancel(&state, "");
    CHECK(!state.approaching);
    CHECK(!CcInteractionInRange(&plan.targets[0], INFINITY, 0));
    puts("interaction planner passed");
    return 0;
}
