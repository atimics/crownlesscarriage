#include "persistence/cc_save.h"
#include "sim/cc_food_economy_internal.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static void CheckReserveUnits(void)
{
    static CcSim sim;
    CcSimInit(&sim, 42U);
    CcSettlement *from = &sim.settlements[0], *to = &sim.settlements[1];
    from->population = 3000;
    from->consumption[CC_GOOD_BREAD] = 5;
    from->service_mask = UINT32_C(1) << CC_SERVICE_INN;
    from->hunger = 0;
    to->hunger = 90;
    from->reserve_target[CC_GOOD_WHEAT] = 120;
    from->reserve_target[CC_GOOD_BREAD] = 100;
    from->reserve_target[CC_GOOD_MEAT] = 100;
    from->reserve_target[CC_GOOD_WOOD] = 100;
    sim.schema_version = 99U;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 120);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_MEAT) == 100);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_BREAD) == 50);
    sim.schema_version = CC_SIM_SCHEMA_VERSION;
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 60);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_MEAT) == 50);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_BREAD) == 50);
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WOOD) == 100);
    CC_CHECK(CcSimHash(&sim) == hash);
    from->service_mask |= UINT32_C(1) << CC_SERVICE_FARM;
    from->cow_adults = 24; from->cow_calves = 0;
    from->sheep_adults = 48; from->sheep_lambs = 0;
    from->pony_adults = 16; from->pony_foals = 0;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 66);
    from->hunger = 35;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 120);
    from->hunger = 0; to->hunger = 64;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 120);
    to->hunger = 90; to->population = 0;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 120);
    to->population = 3000;
    from->reserve_target[CC_GOOD_WHEAT] = 40;
    CC_CHECK(CcEconomyReliefReserve(&sim, from, to, CC_GOOD_WHEAT) == 40);
}

static void SetupShipment(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0xc4111a9e));
    sim->current_day = 6;
    sim->archives.seat_id = sim->settlements[4].id;
    sim->iron_ledger_reserve = 0;
    for (int i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *town = &sim->settlements[i];
        town->market_coins = 1000;
        town->war_chest = 0;
        town->service_mask = UINT32_C(1) << CC_SERVICE_INN;
        town->cow_adults = town->cow_calves = 0;
        town->sheep_adults = town->sheep_lambs = 0;
        town->pony_adults = town->pony_foals = 0;
        for (int g = 0; g < CC_GOOD_COUNT; ++g) {
            town->stock[g] = 0;
            town->reserve_target[g] = 0;
            town->production[g] = 0;
            town->consumption[g] = 0;
            town->price[g] = 1;
        }
    }
    for (int i = 0; i < sim->route_count; ++i) {
        sim->routes[i].closed = true;
        sim->routes[i].condition = 0;
    }
    sim->routes[0].closed = false;
    sim->routes[0].condition = 100;
    sim->routes[0].security = 100;
    CcSettlement *from = &sim->settlements[0], *to = &sim->settlements[1];
    from->population = 3000;
    from->hunger = 0;
    from->stock[CC_GOOD_WHEAT] = 120;
    from->reserve_target[CC_GOOD_WHEAT] = 120;
    from->consumption[CC_GOOD_BREAD] = 5;
    to->hunger = 90;
    to->reserve_target[CC_GOOD_WHEAT] = 40;
    to->population = 2287;
    for (int i = 0; i < sim->royal_carriage_count; ++i) {
        CcRoyalCarriage *carrier = &sim->royal_carriages[i];
        carrier->next_dispatch_day = 13;
        if (carrier->kingdom_id == to->kingdom_id) {
            carrier->next_dispatch_day = 0;
            carrier->location_id = from->id;
        }
    }
}

static void CheckShipments(void)
{
    static CcSim sim, restored;
    const char *path = "nutrition-relief.ccsave";
    char error[256];
    for (int scenario = 0; scenario < 5; ++scenario) {
        SetupShipment(&sim);
        if (scenario == 0) sim.schema_version = 99U;
        if (scenario == 2) {
            sim.routes[0].closed = true;
            sim.routes[0].condition = 0;
        }
        if (scenario == 3) sim.settlements[1].market_coins = 0;
        if (scenario == 4) {
            for (int i = 0; i < sim.royal_carriage_count; ++i)
                sim.royal_carriages[i].next_dispatch_day = 13;
        }
        CcMoney gold = CcSimTrackedGold(&sim);
        CcMoney payer_before = sim.settlements[1].market_coins;
        CcMoney supplier_before = sim.settlements[0].market_coins;
        int32_t population_before = sim.settlements[1].population;
        int32_t hunger_before = sim.settlements[1].hunger;
        int32_t supplier_stock_before = sim.settlements[0].stock[CC_GOOD_WHEAT];
        CcSimAdvanceDays(&sim, 1);
        int loads = 0, quantity = 0;
        CcId shipment_id = 0;
        for (int i = 0; i < sim.shipment_count; ++i) {
            const CcShipment *load = &sim.shipments[i];
            if (load->origin_id == sim.settlements[0].id &&
                load->final_destination_id == sim.settlements[1].id &&
                load->good == CC_GOOD_WHEAT) {
                ++loads; quantity += load->quantity; shipment_id = load->id;
                CC_CHECK(load->status == CC_SHIPMENT_TRAVELLING);
                CC_CHECK(load->arrival_day > sim.current_day);
            }
        }
        printf("relief case=%d loads=%d wheat=%d supplier=%d recipient=%d\n",
            scenario, loads, quantity, sim.settlements[0].stock[CC_GOOD_WHEAT],
            sim.settlements[1].stock[CC_GOOD_WHEAT]);
        CC_CHECK(loads == (scenario == 1 ? 1 : 0));
        CC_CHECK(CcSimTrackedGold(&sim) == gold);
        if (scenario != 1) continue;
        CC_CHECK(quantity > 0);
        CC_CHECK(population_before > 0 && hunger_before >= 65);
        CC_CHECK(sim.settlements[1].market_coins < payer_before);
        CC_CHECK(sim.settlements[0].market_coins > supplier_before);
        CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] < supplier_stock_before);
        CC_CHECK(quantity <= supplier_stock_before - sim.settlements[0].stock[CC_GOOD_WHEAT]);
        CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] >= 60);
        CC_CHECK(sim.settlements[1].stock[CC_GOOD_WHEAT] == 0);
        const CcShipment *shipment = NULL;
        const CcRoyalCarriage *carrier = NULL;
        for (int i = 0; i < sim.shipment_count; ++i)
            if (sim.shipments[i].id == shipment_id) shipment = &sim.shipments[i];
        for (int i = 0; i < sim.royal_carriage_count; ++i)
            if (sim.royal_carriages[i].active_shipment_id == shipment_id)
                carrier = &sim.royal_carriages[i];
        CC_CHECK(shipment != NULL && shipment->good == CC_GOOD_WHEAT &&
            shipment->quantity == quantity && shipment->origin_id == sim.settlements[0].id &&
            shipment->final_destination_id == sim.settlements[1].id);
        const CcRoute *route = shipment != NULL ? CcSimRoute(&sim, shipment->route_id) : NULL;
        CC_CHECK(route != NULL && route->from_id == sim.settlements[0].id &&
            route->to_id == sim.settlements[1].id);
        CC_CHECK(carrier != NULL && carrier->mode == CC_ROYAL_CARRIAGE_DELIVERING &&
            carrier->active_shipment_id == shipment_id && carrier->route_id == shipment->route_id &&
            carrier->target_id == sim.settlements[1].id);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
        CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        bool arrived = false;
        CcNutritionAccounting nutrition = {0};
        for (int day = 0; day < 14; ++day) {
            CcSimAdvanceDaysWithNutritionAccounting(&sim, 1, &nutrition);
            for (int i = 0; i < sim.shipment_count; ++i)
                if (sim.shipments[i].id == shipment_id &&
                    sim.shipments[i].status == CC_SHIPMENT_ARRIVED) arrived = true;
        }
        CcSimAdvanceDays(&restored, 14);
        CC_CHECK(arrived);
        CC_CHECK(nutrition.towns[1].civilian_units[CC_GOOD_WHEAT] > 0);
        CC_CHECK(sim.settlements[1].hunger < hunger_before);
        CC_CHECK(sim.settlements[1].population > 0);
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    }
    (void)remove(path);
    (void)remove("nutrition-relief.ccsave-wal");
    (void)remove("nutrition-relief.ccsave-shm");
}

int main(void)
{
    CheckReserveUnits();
    CheckShipments();
    return 0;
}
