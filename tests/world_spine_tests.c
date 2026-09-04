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

static bool ShipmentsFollowRoyalCarriages(const CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        const CcSettlement *origin = CcSimSettlement(sim, shipment->origin_id);
        const CcSettlement *destination = CcSimSettlement(
            sim, shipment->destination_id);
        const CcSettlement *final = CcSimSettlement(
            sim, shipment->final_destination_id);
        if (origin == NULL || destination == NULL || final == NULL) {
            return false;
        }
        if (shipment->status != CC_SHIPMENT_TRAVELLING &&
            shipment->status != CC_SHIPMENT_BLOCKED) continue;
        const CcRoyalCarriage *carriage = CcSimRoyalCarriage(
            sim, final->kingdom_id);
        if (carriage == NULL ||
            carriage->active_shipment_id != shipment->id ||
            shipment->quantity >
                CC_ROYAL_CARRIAGE_CARGO_SLOTS *
                    CcGoodDefinitionFor(shipment->good)->
                        freight_units_per_slot) return false;
    }
    return true;
}

int main(void)
{
    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x10203040));
    CcSimInit(&second, UINT32_C(0x50607080));

    CC_CHECK(first.kingdom_count == 3);
    CC_CHECK(CcSimKingdomsAtWar(
        &first, first.kingdoms[0].id, first.kingdoms[1].id));
    CC_CHECK(!CcSimKingdomsAtWar(
        &first, first.kingdoms[0].id, first.kingdoms[2].id));
    CC_CHECK(!CcSimKingdomsAtWar(
        &first, first.kingdoms[1].id, first.kingdoms[2].id));
    CC_CHECK(CcSimKingdomCalling(
        &first, first.kingdoms[0].id) == CC_KINGDOM_CALLING_ROAD);
    CC_CHECK(CcSimKingdomCalling(
        &first, first.kingdoms[1].id) == CC_KINGDOM_CALLING_IRON);
    CC_CHECK(CcSimKingdomCalling(
        &first, first.kingdoms[2].id) == CC_KINGDOM_CALLING_DEEP);
    CC_CHECK(strcmp(
        CcKingdomCallingName(CC_KINGDOM_CALLING_ROAD),
        "Road and Granary") == 0);
    CC_CHECK(strcmp(CcFactionKindName(CC_FACTION_GUILD), "Factors") == 0);
    for (int32_t i = 0; i < first.kingdom_count; ++i) {
        int32_t pressure = CcSimKingdomPressure(
            &first, first.kingdoms[i].id);
        CC_CHECK(pressure >= 0 && pressure <= 100);
    }
    CcSim blocked_roads = first;
    int32_t road_pressure_before = CcSimKingdomPressure(
        &blocked_roads, blocked_roads.kingdoms[0].id);
    blocked_roads.routes[0].condition = 0;
    blocked_roads.routes[0].security = 0;
    blocked_roads.routes[0].closed = true;
    CC_CHECK(CcSimKingdomPressure(
        &blocked_roads, blocked_roads.kingdoms[0].id) == 95);
    CC_CHECK(CcSimKingdomPressure(
        &blocked_roads, blocked_roads.kingdoms[0].id) >
        road_pressure_before);
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
    CC_CHECK(CcSimRouteCrossesKingdomBorder(&first, first.routes[6].id));
    CC_CHECK(!CcSimRouteCrossesKingdomBorder(&first, first.routes[0].id));
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
    int32_t wood_before = capital->stock[CC_GOOD_WOOD];
    int32_t stone_before = capital->stock[CC_GOOD_STONE];
    int32_t material_before = capital->stock[CC_GOOD_MATERIAL];
    int32_t tools_before = capital->stock[CC_GOOD_TOOLS];
    char error[192];
    CcSim short_wood = builder;
    CcSettlement *short_wood_capital = &short_wood.settlements[4];
    short_wood_capital->stock[CC_GOOD_WOOD] = 7;
    CcMoney short_wood_treasury = short_wood.kingdoms[2].treasury;
    int32_t short_wood_iron = short_wood_capital->stock[CC_GOOD_IRON];
    int32_t short_wood_tools = short_wood_capital->stock[CC_GOOD_TOOLS];
    CC_CHECK(!CcSimStartServiceProject(
        &short_wood, short_wood_capital->id, CC_SERVICE_GRANARY,
        error, sizeof(error)));
    CC_CHECK(strstr(error, "8 Wood") != NULL);
    CC_CHECK(short_wood.kingdoms[2].treasury == short_wood_treasury);
    CC_CHECK(short_wood_capital->stock[CC_GOOD_IRON] == short_wood_iron);
    CC_CHECK(short_wood_capital->stock[CC_GOOD_TOOLS] == short_wood_tools);
    CcSim short_stone = builder;
    CcSettlement *short_stone_capital = &short_stone.settlements[4];
    short_stone_capital->stock[CC_GOOD_STONE] = 5;
    CcMoney short_stone_treasury = short_stone.kingdoms[2].treasury;
    int32_t short_stone_wood = short_stone_capital->stock[CC_GOOD_WOOD];
    int32_t short_stone_iron = short_stone_capital->stock[CC_GOOD_IRON];
    int32_t short_stone_tools = short_stone_capital->stock[CC_GOOD_TOOLS];
    CC_CHECK(!CcSimStartServiceProject(
        &short_stone, short_stone_capital->id, CC_SERVICE_GRANARY,
        error, sizeof(error)));
    CC_CHECK(strstr(error, "6 Stone") != NULL);
    CC_CHECK(short_stone.kingdoms[2].treasury == short_stone_treasury);
    CC_CHECK(short_stone_capital->stock[CC_GOOD_WOOD] == short_stone_wood);
    CC_CHECK(short_stone_capital->stock[CC_GOOD_IRON] == short_stone_iron);
    CC_CHECK(short_stone_capital->stock[CC_GOOD_TOOLS] == short_stone_tools);
    CC_CHECK(CcSimStartServiceProject(&builder, capital->id,
                                      CC_SERVICE_GRANARY,
                                      error, sizeof(error)));
    CC_CHECK(builder.kingdoms[2].treasury == treasury_before - 80);
    CC_CHECK(capital->stock[CC_GOOD_WOOD] == wood_before - 8);
    CC_CHECK(capital->stock[CC_GOOD_STONE] == stone_before - 6);
    CC_CHECK(capital->stock[CC_GOOD_MATERIAL] == material_before - 6);
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
    carriage.maps[1].owner_id = carriage.player.id;
    CcCommand cross_border = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = carriage.settlements[2].id
    };
    CC_CHECK(CcSimRouteCrossesWarBorder(&carriage,
                                        carriage.routes[1].id));
    CC_CHECK(CcSimApply(&carriage, &cross_border, error, sizeof(error)));

    CcSimAdvanceDays(&first, 55);
    CC_CHECK(first.shipment_count > 0);
    CC_CHECK(ShipmentsFollowRoyalCarriages(&first));
    for (int32_t i = 0; i < first.shipment_count; ++i) {
        const CcShipment *shipment = &first.shipments[i];
        const CcSettlement *origin = CcSimSettlement(&first, shipment->origin_id);
        const CcSettlement *destination = CcSimSettlement(
            &first, shipment->destination_id);
        const CcSettlement *final = CcSimSettlement(
            &first, shipment->final_destination_id);
        const CcRoute *shipment_route = CcSimRoute(
            &first, shipment->route_id);
        CC_CHECK(origin != NULL && destination != NULL && final != NULL);
        CC_CHECK(shipment_route != NULL);
        CC_CHECK(shipment_route->from_id == origin->id ||
                 shipment_route->to_id == origin->id);
        CC_CHECK(shipment_route->from_id == destination->id ||
                 shipment_route->to_id == destination->id);
    }

    CcSim peaceful_smuggling;
    CcSimInit(&peaceful_smuggling, UINT32_C(0x15b04de7));
    CcRoute *foreign_route = &peaceful_smuggling.routes[6];
    const CcSettlement *foreign_from = CcSimSettlement(
        &peaceful_smuggling, foreign_route->from_id);
    const CcSettlement *foreign_to = CcSimSettlement(
        &peaceful_smuggling, foreign_route->to_id);
    CC_CHECK(foreign_from != NULL && foreign_to != NULL);
    int32_t foreign_from_kingdom = -1;
    int32_t foreign_to_kingdom = -1;
    for (int32_t i = 0; i < peaceful_smuggling.kingdom_count; ++i) {
        if (peaceful_smuggling.kingdoms[i].id == foreign_from->kingdom_id) {
            foreign_from_kingdom = i;
        }
        if (peaceful_smuggling.kingdoms[i].id == foreign_to->kingdom_id) {
            foreign_to_kingdom = i;
        }
    }
    CC_CHECK(foreign_from_kingdom >= 0 && foreign_to_kingdom >= 0);
    peaceful_smuggling.diplomacy[foreign_from_kingdom][foreign_to_kingdom] =
        CC_DIPLOMACY_PEACE;
    peaceful_smuggling.diplomacy[foreign_to_kingdom][foreign_from_kingdom] =
        CC_DIPLOMACY_PEACE;
    foreign_route->smuggler_route = true;
    CC_CHECK(!CcSimRouteCrossesWarBorder(&peaceful_smuggling,
                                         foreign_route->id));
    CC_CHECK(CcSimRouteCrossesKingdomBorder(&peaceful_smuggling,
                                            foreign_route->id));
    CcSimAdvanceDays(&peaceful_smuggling, 55);
    CC_CHECK(peaceful_smuggling.shipment_count > 0);
    CC_CHECK(ShipmentsFollowRoyalCarriages(&peaceful_smuggling));

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
