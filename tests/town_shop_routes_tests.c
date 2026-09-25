#include "client/cc_local3d.h"
#include "client/cc_local_shops.h"

#include <math.h>
#include <stdio.h>

static bool WalkTo(CcLocalAgent *agent, Vector2 destination, float radius,
                   const char *town, const char *shop, const char *leg)
{
    if (!CcLocalAgentApproachInteraction(agent, destination, radius, false)) {
        (void)fprintf(stderr, "%s %s: %s route cannot start at %.2f,%.2f toward %.2f,%.2f\n",
            town, shop, leg, agent->position.x, agent->position.z,
            destination.x, destination.y);
        return false;
    }
    for (int32_t frame = 0; frame < 2400; ++frame) {
        float distance = hypotf(agent->position.x - destination.x,
                                agent->position.z - destination.y);
        if (distance <= radius * 0.95f) {
            CcLocalAgentStop(agent);
            return true;
        }
        CcLocalAgentUpdate(agent, 0.05f, false);
        if (!agent->navigation_active && !agent->exact_target_valid) break;
    }
    float distance = hypotf(agent->position.x - destination.x,
                            agent->position.z - destination.y);
    if (distance <= radius) return true;
    (void)fprintf(stderr, "%s %s: %s stopped %.2f away at %.2f,%.2f (target %.2f,%.2f)\n",
        town, shop, leg, distance, agent->position.x, agent->position.z,
        destination.x, destination.y);
    return false;
}

int main(void)
{
    static CcSim sim;
    const uint32_t seeds[] = {UINT32_C(0xc0a71a9e), UINT32_C(0x12345678)};
    const Vector2 carriage = {CC_LOCAL_CARRIAGE_APPROACH_X, CC_LOCAL_CARRIAGE_APPROACH_Z};
    int32_t journeys = 0;
    int32_t failures = 0;
    CcLocalBindOpenWorld(NULL);
    CcLocalRendererSetOpeningStep(CC_LOCAL_OPENING_COMPLETE);
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        CcSimInit(&sim, seeds[seed]);
        for (int32_t town = 0; town < sim.settlement_count; ++town) {
            sim.player.location_id = sim.settlements[town].id;
            CcLocalBindPlace(&sim);
            const CcLocalPlaceProfile *profile =
                CcLocalPlaceProfileForSettlement(&sim.settlements[town]);
            for (int32_t kind = 0; kind < CC_LOCAL_SHOP_COUNT; ++kind) {
                const CcLocalShop *shop = CcLocalShopAt(profile, (CcLocalShopKind)kind);
                if (shop == NULL) {
                    (void)fprintf(stderr, "%s: shop %d needs a building\n",
                        sim.settlements[town].name, kind);
                    failures += 1;
                    continue;
                }
                CcLocalLanePoint approach = CcLocalShopApproach(profile, shop);
                CcLocalAgent agent;
                CcLocalAgentInit(&agent, carriage, false);
                if (!WalkTo(&agent, (Vector2){approach.x, approach.z}, 1.1f,
                            sim.settlements[town].name, shop->name, "outward")) {
                    failures += 1;
                    continue;
                }
                /* Leaving the interior starts at the same shop's front door. */
                CcLocalAgentInit(&agent, (Vector2){approach.x, approach.z}, false);
                if (!WalkTo(&agent, carriage, 1.5f,
                            sim.settlements[town].name, shop->name, "return")) {
                    failures += 1;
                    continue;
                }
                journeys += 1;
            }
        }
    }
    CcLocalBindPlace(NULL);
    if (failures > 0) return 1;
    (void)printf("PASS town shops: %d walks from the carriage and back across six towns and two seeds\n", journeys);
    return 0;
}
