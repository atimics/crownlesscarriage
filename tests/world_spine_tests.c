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

    CC_CHECK(first.kingdom_count == 3);
    for (int32_t a = 0; a < first.kingdom_count; ++a) {
        for (int32_t b = a + 1; b < first.kingdom_count; ++b) {
            CC_CHECK(CcSimKingdomsAtWar(
                &first, first.kingdoms[a].id, first.kingdoms[b].id));
        }
    }
    for (int32_t i = 0; i < first.settlement_count; ++i) {
        const CcSettlement *settlement = &first.settlements[i];
        CC_CHECK(CcSettlementServiceCount(settlement) > 0);
        CC_CHECK(CcSettlementServiceCount(settlement) <=
                 CcSettlementServiceCapacity(settlement->size));
        for (int32_t other = i + 1; other < first.settlement_count; ++other) {
            CC_CHECK(settlement->service_mask !=
                     first.settlements[other].service_mask);
        }
    }
    CC_CHECK(CcSimRouteCrossesWarBorder(&first, first.routes[6].id));
    CC_CHECK(!CcSimRouteCrossesWarBorder(&first, first.routes[0].id));
    CC_CHECK(CcSettlementHasService(&first.settlements[4],
                                    CC_SERVICE_GUILDHALL));
    CC_CHECK(!CcSettlementHasService(&first.settlements[4],
                                     CC_SERVICE_GRANARY));

    CC_CHECK(strcmp(first.settlements[0].name, second.settlements[0].name) != 0 ||
           first.settlements[0].map_x != second.settlements[0].map_x ||
           first.settlements[0].map_y != second.settlements[0].map_y);
    CC_CHECK(CcSimHash(&first) != CcSimHash(&second));
    CC_CHECK(CcSimActiveSituationCount(&first) >= 3);
    CC_CHECK(CcSimSituationForSettlement(&first, first.settlements[3].id) != NULL);
    CC_CHECK(CcSimRouteDanger(&first, first.routes[6].id) >
           CcSimRouteDanger(&first, first.routes[0].id));

    CcSim builder;
    CcSimInit(&builder, UINT32_C(0xb017de12));
    CcSettlement *capital = &builder.settlements[4];
    CcMoney treasury_before = builder.kingdoms[2].treasury;
    int32_t material_before = capital->stock[CC_GOOD_MATERIAL];
    int32_t tools_before = capital->stock[CC_GOOD_TOOLS];
    char error[192];
    CC_CHECK(CcSimStartServiceProject(&builder, capital->id,
                                      CC_SERVICE_GRANARY,
                                      error, sizeof(error)));
    CC_CHECK(builder.kingdoms[2].treasury == treasury_before - 80);
    CC_CHECK(capital->stock[CC_GOOD_MATERIAL] == material_before - 12);
    CC_CHECK(capital->stock[CC_GOOD_TOOLS] == tools_before - 5);
    CcSimAdvanceDays(&builder, 6);
    CC_CHECK(!CcSettlementHasService(capital, CC_SERVICE_GRANARY));
    CcSimAdvanceDays(&builder, 1);
    CC_CHECK(CcSettlementHasService(capital, CC_SERVICE_GRANARY));
    CC_CHECK(CountEvents(&builder, CC_EVENT_SERVICE_OPENED) == 1);

    CcSim raid;
    CcSimInit(&raid, UINT32_C(0xbaad17));
    CcBanditGroup *raiders = &raid.bandits[0];
    CC_CHECK(CcSimRouteCrossesWarBorder(&raid, raiders->route_id));
    CC_CHECK(CcSimLaunchBanditRaid(&raid, raiders->id,
                                  error, sizeof(error)));
    CC_CHECK(raiders->raid_phase == CC_BANDIT_RAID_SCOUTING);
    while (raiders->raid_phase != CC_BANDIT_RAID_OUTBOUND) {
        CcSimAdvanceDays(&raid, 1);
    }
    while (raiders->raid_days_remaining > 1) {
        CcSimAdvanceDays(&raid, 1);
    }
    CcSettlement *raid_target = CcSimSettlementMutable(
        &raid, raiders->raid_target_id);
    CC_CHECK(raid_target != NULL);
    int32_t stock_before_raid = raid_target->stock[raiders->raid_good];
    CcSimAdvanceDays(&raid, 1);
    CC_CHECK(raiders->raid_phase == CC_BANDIT_RAID_RETURNING);
    CC_CHECK(raiders->raid_quantity > 0);
    CC_CHECK(raid_target->stock[raiders->raid_good] ==
             stock_before_raid - raiders->raid_quantity);
    int32_t stolen = raiders->raid_quantity;
    int32_t supplies_before_return = raiders->supplies;
    while (raiders->raid_phase != CC_BANDIT_RAID_IDLE) {
        CcSimAdvanceDays(&raid, 1);
    }
    CC_CHECK(raiders->raids_completed == 1);
    CC_CHECK(raiders->supplies == supplies_before_return + stolen);
    CC_CHECK(CountEvents(&raid, CC_EVENT_SETTLEMENT_RAIDED) == 1);
    CC_CHECK(CountEvents(&raid, CC_EVENT_BANDIT_RAID_RETURNED) == 1);

    CcSim carriage;
    CcSimInit(&carriage, UINT32_C(0xca771a9e));
    carriage.player.location_id = carriage.settlements[1].id;
    carriage.carriage.location_id = carriage.player.location_id;
    carriage.maps[5].owner_id = carriage.player.id;
    CcCommand cross_border = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = carriage.settlements[5].id
    };
    CC_CHECK(CcSimRouteCrossesWarBorder(&carriage,
                                        carriage.routes[5].id));
    CC_CHECK(CcSimApply(&carriage, &cross_border, error, sizeof(error)));

    CcSimAdvanceDays(&first, 55);
    for (int32_t i = 0; i < first.shipment_count; ++i) {
        const CcShipment *shipment = &first.shipments[i];
        const CcSettlement *origin = CcSimSettlement(&first, shipment->origin_id);
        const CcSettlement *destination = CcSimSettlement(
            &first, shipment->destination_id);
        const CcSettlement *final = CcSimSettlement(
            &first, shipment->final_destination_id);
        CC_CHECK(origin != NULL && destination != NULL && final != NULL);
        CC_CHECK(origin->kingdom_id == destination->kingdom_id);
        CC_CHECK(origin->kingdom_id == final->kingdom_id);
    }

    int32_t initial_support[CC_MAX_FACTIONS];
    for (int32_t i = 0; i < first.faction_count; ++i) {
        initial_support[i] = first.factions[i].support;
    }
    CcMoney initial_treasury = first.kingdoms[0].treasury;
    CcSimAdvanceDays(&first, 29);

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
