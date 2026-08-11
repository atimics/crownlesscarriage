#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

int main(void)
{
    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x10203040));
    CcSimInit(&second, UINT32_C(0x50607080));

    CC_CHECK(strcmp(first.settlements[0].name, second.settlements[0].name) != 0 ||
           first.settlements[0].map_x != second.settlements[0].map_x ||
           first.settlements[0].map_y != second.settlements[0].map_y);
    CC_CHECK(CcSimHash(&first) != CcSimHash(&second));
    CC_CHECK(CcSimActiveSituationCount(&first) >= 3);
    CC_CHECK(CcSimSituationForSettlement(&first, first.settlements[3].id) != NULL);
    CC_CHECK(CcSimRouteDanger(&first, first.routes[6].id) >
           CcSimRouteDanger(&first, first.routes[0].id));

    CcSimAdvanceDays(&first, 55);
    bool has_multileg_freight = false;
    for (int32_t i = 0; i < first.shipment_count; ++i) {
        const CcShipment *shipment = &first.shipments[i];
        if (shipment->status == CC_SHIPMENT_TRAVELLING &&
            shipment->destination_id != shipment->final_destination_id) {
            has_multileg_freight = true;
        }
    }
    CC_CHECK(has_multileg_freight);

    int32_t initial_support[CC_MAX_FACTIONS];
    for (int32_t i = 0; i < first.faction_count; ++i) {
        initial_support[i] = first.factions[i].support;
    }
    CcMoney initial_treasury = first.kingdoms[0].treasury;
    CcSimAdvanceDays(&first, 29);

    char error[192];
    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    bool politics_changed = false;
    for (int32_t i = 0; i < first.faction_count; ++i) {
        if (first.factions[i].support != initial_support[i]) politics_changed = true;
    }
    CC_CHECK(politics_changed);
    CC_CHECK(first.kingdoms[0].treasury != initial_treasury);
    CC_CHECK(CountEvents(&first, CC_EVENT_FACTION_SHIFT) >= 3);
    CC_CHECK(CountEvents(&first, CC_EVENT_KINGDOM_ACTION) >= 1);
    CC_CHECK(CountEvents(&first, CC_EVENT_SITUATION_FAILED) >= 1);

    bool has_causal_child = false;
    for (int32_t i = 0; i < first.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&first, i);
        if (event != NULL && event->parent_id != 0U &&
            (event->kind == CC_EVENT_SHIPMENT_ARRIVED ||
             event->kind == CC_EVENT_SHIPMENT_LOST ||
             event->kind == CC_EVENT_KINGDOM_ACTION)) {
            has_causal_child = true;
        }
    }
    CC_CHECK(has_causal_child);

    for (uint32_t seed = 1U; seed <= 8U; ++seed) {
        CcSim sim;
        CcSimInit(&sim, seed * UINT32_C(0x9e3779b9));
        for (int32_t quarter = 0; quarter < 12; ++quarter) {
            CcSimAdvanceDays(&sim, 91);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        }
        int32_t total_hunger = 0;
        for (int32_t i = 0; i < sim.settlement_count; ++i) {
            total_hunger += sim.settlements[i].hunger;
        }
        CC_CHECK(total_hunger / sim.settlement_count < 70);
        CC_CHECK(sim.monsters[0].pressure < 95);
    }

    puts("Living-world feedback tests passed");
    return 0;
}
