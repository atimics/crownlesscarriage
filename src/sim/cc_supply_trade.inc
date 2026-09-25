/* Producer buybacks stay below the fixed source price. Paid workshop orders
   have their own bounded quote. */
int32_t CcSimTradeResalePrice(const CcSim *sim, const CcSettlement *town, CcGood good)
{
    if (sim == NULL || town == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    if (sim->schema_version >= 117U &&
        (good == CC_GOOD_WHEAT || good == CC_GOOD_RAW_STONE)) return 2;
    return MaximumI32(1, town->price[good] * 3 / 4);
}

/* Producer stock and finished workshop goods are shared by every player. */
void CcSimInitializeSupplyEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *town = &sim->settlements[i];
        bool mine = town->function == CC_SETTLEMENT_MINING;
        town->stock[CC_GOOD_RAW_STONE] = mine ? 24 : 0;
        town->reserve_target[CC_GOOD_RAW_STONE] = mine ? 8 : 0;
        town->production[CC_GOOD_RAW_STONE] = mine ? 6 : 0;
        town->consumption[CC_GOOD_RAW_STONE] = 0;
        town->price[CC_GOOD_RAW_STONE] = 3;
    }
}

CcSupplyOffer CcSimSupplyOffer(const CcSim *sim, CcGood good)
{
    CcSupplyOffer offer = {0};
    if (sim == NULL || sim->schema_version < 117U ||
        (good != CC_GOOD_WHEAT && good != CC_GOOD_RAW_STONE)) return offer;
    offer.source_unit_price = 3;
    offer.delivery_unit_price = good == CC_GOOD_WHEAT ? 5 : 6;
    const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
    if (town == NULL || sim->journey.active || town->population <= 0 ||
        town->fire_damage >= 80) return offer;
    bool wheat = good == CC_GOOD_WHEAT;
    bool source = wheat || (town->function == CC_SETTLEMENT_MINING &&
        CcSettlementHasService(town, CC_SERVICE_MINE));
    offer.source_available = source ? town->stock[good] : 0;
    offer.ready = true;
    CcGood output = wheat ? CC_GOOD_BREAD : CC_GOOD_STONE;
    int32_t next_use = wheat ? CcEconomyWeeklyFoodUse(sim, town) :
        town->consumption[CC_GOOD_STONE];
    int32_t target = MinimumI32(CC_SIM_MAX_UNITS,
        town->reserve_target[output] + next_use);
    offer.remaining = MaximumI32(0, target - town->stock[output]);
    CcMoney funds = town->market_coins / offer.delivery_unit_price;
    if (funds < offer.remaining) offer.remaining = (int32_t)funds;
    return offer;
}

static bool ApplySupplyTrade(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcSupplyOffer offer = CcSimSupplyOffer(sim, command->good);
    CcSettlement *town = CcSimSettlementMutable(sim, sim->player.location_id);
    if (town == NULL || command->target_id != town->id ||
        offer.source_unit_price == 0 || command->amount == 0 ||
        command->amount == INT32_MIN ||
        command->amount > CC_SIM_MAX_UNITS || command->amount < -CC_SIM_MAX_UNITS) {
        SetError(error, error_capacity, "Choose a current local supply offer.");
        return false;
    }
    bool buying = command->amount > 0;
    int32_t units = buying ? command->amount : -command->amount;
    CcMoney coins = (CcMoney)units * (buying ?
        offer.source_unit_price : offer.delivery_unit_price);
    if (buying) {
        if (units > offer.source_available || sim->player.coins < coins ||
            town->market_coins > CC_SIM_MAX_MONEY - coins ||
            sim->player.cargo[command->good] > CC_SIM_MAX_UNITS - units) {
            SetError(error, error_capacity, "Review the seller's stock and the available crowns.");
            return false;
        }
        int32_t before = CcGoodsPlayerCargoBoxes(command->good,
            sim->player.cargo[command->good]);
        int32_t after = CcGoodsPlayerCargoBoxes(command->good,
            sim->player.cargo[command->good] + units);
        if (CcPlayerCargoUsed(&sim->player) - before + after > sim->player.cargo_capacity) {
            SetError(error, error_capacity, "Make room in the carriage for this load.");
            return false;
        }
        town->stock[command->good] -= units;
        sim->player.cargo[command->good] += units;
        sim->player.coins -= coins;
        town->market_coins += coins;
    } else {
        if (!offer.ready || units > offer.remaining ||
            sim->player.cargo[command->good] < units ||
            sim->player.coins > CC_SIM_MAX_MONEY - coins) {
            SetError(error, error_capacity, "Review the workshop's remaining order and your load.");
            return false;
        }
        /* Reserved charter cargo keeps its existing delivery contract. */
        const CcSituation *accepted = CcSimAcceptedSituation(sim);
        if (accepted != NULL && accepted->good == command->good &&
            (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
             accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
            SetError(error, error_capacity, "Finish the promised consignment before taking a local order.");
            return false;
        }
        sim->player.cargo[command->good] -= units;
        town->market_coins -= coins;
        sim->player.coins += coins;
        /* The order includes workshop labor: grain becomes bread and raw
           stone becomes building stone at this buyer's actual location. */
        CcGood output = command->good == CC_GOOD_WHEAT ? CC_GOOD_BREAD : CC_GOOD_STONE;
        town->stock[output] += units;
        CcEconomyRefreshSettlementGoodPrice(sim, town, output);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    const char *buyer = command->good == CC_GOOD_WHEAT ? "Bakery" : "Stonecutter";
    (void)snprintf(text, sizeof(text), buying ?
        "The company buys %d %s for %" PRId64 " crowns; %s pays %d per unit." :
        "The company delivers %d %s for %" PRId64 " crowns; %s pays %d per unit.",
        units, CcGoodName(command->good), coins, buyer, offer.delivery_unit_price);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRADE, sim->player.id, town->id,
                    0U, units, text);
    CcEconomyRefreshSettlementGoodPrice(sim, town, command->good);
    SetError(error, error_capacity, "");
    return true;
}
