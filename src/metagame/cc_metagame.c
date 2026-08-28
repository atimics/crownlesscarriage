#include "metagame/cc_metagame.h"

#include "persistence/cc_save.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Append(char *output, size_t capacity, const char *format, ...)
{
    if (output == NULL || capacity == 0U) return;
    size_t used = strlen(output);
    if (used >= capacity - 1U) return;
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(output + used, capacity - used, format, arguments);
    va_end(arguments);
}

static bool ParseIndex(const char *text, int32_t maximum, int32_t *index)
{
    if (text == NULL || index == NULL) return false;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > maximum) {
        return false;
    }
    *index = (int32_t)value - 1;
    return true;
}

static bool ParseDays(const char *text, int32_t *days)
{
    if (text == NULL || days == NULL) return false;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 365) return false;
    *days = (int32_t)value;
    return true;
}

static bool ParseAmount(const char *text, int32_t *amount)
{
    if (text == NULL || amount == NULL) return false;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > INT32_MAX) {
        return false;
    }
    *amount = (int32_t)value;
    return true;
}

static bool ParseGood(const char *text, CcGood *good)
{
    if (text == NULL || good == NULL) return false;
    if (strcmp(text, "food") == 0) *good = CC_GOOD_FOOD;
    else if (strcmp(text, "iron") == 0 || strcmp(text, "material") == 0) {
        *good = CC_GOOD_IRON;
    }
    else if (strcmp(text, "tools") == 0) *good = CC_GOOD_TOOLS;
    else if (strcmp(text, "weapons") == 0) *good = CC_GOOD_WEAPONS;
    else if (strcmp(text, "gold") == 0) *good = CC_GOOD_GOLD;
    else if (strcmp(text, "gems") == 0) *good = CC_GOOD_GEMS;
    else return false;
    return true;
}

static const CcSettlement *CurrentPlace(const CcMetagame *metagame)
{
    return CcSimSettlement(&metagame->sim,
                           metagame->sim.player.location_id);
}

static const CcCourier *CourierById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].id == id) return &sim->couriers[i];
    }
    return NULL;
}

static const char *CourierPurpose(CcCourierKind kind)
{
    switch (kind) {
        case CC_COURIER_WAR_DECLARATION: return "a declaration of war";
        case CC_COURIER_PEACE_OFFER: return "an offer of peace";
        case CC_COURIER_DRAGON_ALLIANCE: return "a call for dragon alliance";
        case CC_COURIER_DRAGON_MUSTER: return "orders to muster against the dragon";
    }
    return "sealed news";
}

static const char *SituationTarget(const CcSim *sim,
                                   const CcSituation *situation,
                                   char *buffer, size_t capacity)
{
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        const CcSettlement *place = CcSimSettlement(sim, situation->target_id);
        return place != NULL ? place->name : "an unknown settlement";
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        if (route != NULL) {
            const CcSettlement *from = CcSimSettlement(sim, route->from_id);
            const CcSettlement *to = CcSimSettlement(sim, route->to_id);
            (void)snprintf(buffer, capacity, "%s to %s",
                           from != NULL ? from->name : "unknown",
                           to != NULL ? to->name : "unknown");
            return buffer;
        }
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].id == situation->target_id) {
                return sim->dungeons[i].name;
            }
        }
    }
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        const CcCourier *courier = CourierById(sim, situation->target_id);
        if (courier != NULL) {
            const CcSettlement *destination = CcSimSettlement(
                sim, courier->destination_settlement_id);
            (void)snprintf(buffer, capacity, "%s to %s",
                           CourierPurpose(courier->kind),
                           destination != NULL ? destination->name :
                           "an unknown court");
            return buffer;
        }
    }
    return "an unknown target";
}

static void DescribeLook(const CcMetagame *metagame,
                         char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    if (place == NULL) {
        Append(output, capacity, "The carriage is between known places.\n");
        return;
    }
    Append(output, capacity, "\n%s — %s, day %d\n",
           place->name, CcSettlementFunctionName(place->function),
           sim->current_day);
    if (place->stock[CC_GOOD_FOOD] < place->reserve_target[CC_GOOD_FOOD] / 2) {
        Append(output, capacity,
               "Empty baskets line the market. A guard keeps people back from the granary door.\n");
    } else if (place->stock[CC_GOOD_FOOD] <
               place->reserve_target[CC_GOOD_FOOD]) {
        Append(output, capacity,
               "Bread is still sold, but each queue is longer than the last.\n");
    } else {
        Append(output, capacity,
               "The market has food today, though road news keeps every bargain tense.\n");
    }
    if (place->hunger >= 40) {
        Append(output, capacity,
               "Families wait beside the stalls after their coins are refused.\n");
    } else if (place->hunger >= 15) {
        Append(output, capacity,
               "Workers split small loaves before returning to their shifts.\n");
    }
    if (place->id == sim->dragon.lair_settlement_id) {
        Append(output, capacity,
               "A black cave overlooks the roofs. Goblins climb to it with wrapped gifts.\n");
    }
    if (place->id == sim->dragon.retaliation_target_id &&
        sim->dragon.stolen_outstanding > 0) {
        if (sim->dragon.omen_days_remaining <= 3) {
            Append(output, capacity,
                   "Warm ash falls from a clear sky. Roof tiles are hot to the touch.\n");
        } else if (sim->dragon.omen_days_remaining <= 7) {
            Append(output, capacity,
                   "The wells steam at dawn. Livestock pull their tethers toward the road.\n");
        } else {
            Append(output, capacity,
                   "Smoke sinks into cold chimneys. Dogs and birds have fled the road.\n");
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            CcSimSituationOfferSettlementId(sim, situation) != place->id) continue;
        Append(output, capacity,
               "%s is asking carriers to act. %s will live with the result.\n",
               situation->sponsor_name, situation->affected_name);
    }
    Append(output, capacity,
           "Type 'causes' to trace the trouble, or 'help' for all actions.\n");
}

static void DescribeCauses(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    const CcSituation *chosen = NULL;
    CcId event_id = 0U;
    if (place != NULL && sim->dragon.stolen_outstanding > 0 &&
        sim->dragon.retaliation_target_id == place->id) {
        event_id = sim->dragon.omen_event_id;
    }
    if (event_id == 0U && place != NULL &&
        sim->hoard_raiders.origin_settlement_id == place->id) {
        event_id = sim->hoard_raiders.cause_event_id;
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->cause_event_id == 0U) continue;
        if (place != NULL &&
            CcSimSituationOfferSettlementId(sim, situation) == place->id) {
            chosen = situation;
            break;
        }
    }
    if (event_id == 0U && chosen != NULL) event_id = chosen->cause_event_id;
    if (event_id == 0U) {
        Append(output, capacity, "No clear local cause has been recorded yet.\n");
        return;
    }
    Append(output, capacity, "The short chain people can see:\n");
    for (int32_t link = 0; link < 6 && event_id != 0U; ++link) {
        const CcEvent *event = CcSimEvent(sim, event_id);
        if (event == NULL) break;
        Append(output, capacity, "  %d. %s\n", link + 1, event->text);
        event_id = event->parent_id;
    }
}

static void DescribePeople(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    Append(output, capacity, "People you can meet here:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE || place == NULL ||
            CcSimSituationOfferSettlementId(sim, situation) != place->id) continue;
        char target[96];
        const char *target_name = SituationTarget(
            sim, situation, target, sizeof(target));
        Append(output, capacity, "  %s backs %s at %s. %s bears the cost.\n",
               situation->sponsor_name,
               CcSituationKindName(situation->kind), target_name,
               situation->affected_name);
        shown += 1;
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcRoute *route = CcSimRoute(sim, sim->bandits[i].route_id);
        if (place != NULL && route != NULL &&
            (route->from_id == place->id || route->to_id == place->id)) {
            Append(output, capacity,
                   "  %s offers movement outside the law, and grows stronger when paid.\n",
                   sim->bandits[i].name);
            shown += 1;
        }
    }
    if (place != NULL &&
        (place->id == sim->dragon.lair_settlement_id ||
         place->id == sim->goblins.tribute_target_id)) {
        Append(output, capacity,
               "  %s worships %s and carries stolen trinkets to its cave.\n",
               sim->goblins.name, sim->dragon.name);
        shown += 1;
    }
    if (place != NULL && place->id ==
            sim->hoard_raiders.origin_settlement_id) {
        if (sim->hoard_raiders.motive == CC_HOARD_RAID_WAR_FINANCE) {
            Append(output, capacity,
                   "  %s carries sealed orders to fund the border war with the dragon's gold.\n",
                   sim->hoard_raiders.name);
        } else {
            Append(output, capacity,
                   "  %s means to answer hunger with the dragon's guarded gold.\n",
                   sim->hoard_raiders.name);
        }
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  No one here offers a clear bargain.\n");
}

static void DescribeRumors(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    Append(output, capacity, "Rumors heard in %s:\n",
           place != NULL ? place->name : "the road");
    if (place != NULL && place->id == sim->settlements[1].id) {
        Append(output, capacity,
               "  A miller says the western farms still have grain.\n"
               "  A clerk says the sanctioned bridge still passes carts for a toll.\n"
               "  A night driver says an older road reaches the mine without a gate.\n");
    } else if (place != NULL && place->id == sim->settlements[0].id) {
        Append(output, capacity,
               "  Carters say eastern workers are being turned away from empty stores.\n"
               "  The fortress has sanctioned the treaty bridge; relief papers lower the trouble, not the toll.\n");
    } else if (place != NULL && place->id == sim->settlements[3].id) {
        Append(output, capacity,
               "  Miners blame the reopened deep works for the creatures below.\n"
               "  Officials, smugglers, and wardens each want a different future for the tunnel.\n");
    } else {
        Append(output, capacity,
               "  Road talk points back toward the hungry market and the tolled bridge.\n");
    }
    Append(output, capacity,
           "  Old folk agree: goblins feed the hoard; the dragon burns only when treasure is stolen from it.\n");
    Append(output, capacity,
           "  Hungry people who can see guarded wealth have begun asking why only goblins dare the cave.\n");
    int32_t heard = 0;
    for (int32_t i = 0; i < sim->event_count && heard < 4; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL || place == NULL || event->location_id != place->id ||
            sim->current_day - event->day > 112) continue;
        bool courier_news = event->kind == CC_EVENT_COURIER_ARRIVED ||
            event->kind == CC_EVENT_COURIER_LOST ||
            event->kind == CC_EVENT_COURIER_DISTORTED ||
            event->kind == CC_EVENT_WAR_DECLARED ||
            event->kind == CC_EVENT_PEACE_DECLARED ||
            event->kind == CC_EVENT_ALLIANCE_DECLARED ||
            event->kind == CC_EVENT_DRAGON_MUSTERED ||
            event->kind == CC_EVENT_DRAGON_BATTLE ||
            event->kind == CC_EVENT_DRAGON_SLAIN ||
            event->kind == CC_EVENT_DRAGON_HOARD_RECOVERED;
        if (!courier_news) continue;
        Append(output, capacity, "  Day %d: %s\n", event->day, event->text);
        heard += 1;
    }
}

static void DescribeCharters(const CcMetagame *metagame,
                             char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    Append(output, capacity, "Offers made here:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            (CcSimSituationOfferSettlementId(sim, situation) !=
                 sim->player.location_id &&
             (accepted == NULL || accepted->id != situation->id))) continue;
        char target[96];
        const char *target_name = SituationTarget(
            sim, situation, target, sizeof(target));
        Append(output, capacity,
               "  %d. %s — %s; reward %" PRId64
               ", deadline day %d%s\n",
               i + 1, CcSituationKindName(situation->kind), target_name,
               situation->reward, situation->deadline_day,
               accepted != NULL && accepted->id == situation->id ?
                   " [accepted]" : "");
        shown += 1;
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            Append(output, capacity, "     Deliver %d %s for %s.\n",
                   situation->quantity, CcGoodName(situation->good),
                   situation->affected_name);
        } else if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
            Append(output, capacity,
                   "     Reopen the road with 2 tools or 18 crowns. Tools are faster and last longer.\n");
        } else if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
            Append(output, capacity,
                   "     Public costs 2 tools + 12 crowns; smuggler costs 1 tool + 6; seal costs 3 tools.\n");
        } else {
            Append(output, capacity,
                   "     Carry the sealed dispatch in the carriage. Its news takes effect only when it reaches the named court.\n");
        }
    }
    if (shown == 0) Append(output, capacity, "  No open offer can be made here.\n");
    Append(output, capacity,
           "Use 'accept NUMBER' or 'refuse NUMBER'. Only one promise fits at a time.\n");
}

static const char *DangerWord(int32_t danger)
{
    if (danger < 20) return "quiet";
    if (danger < 40) return "uneasy";
    if (danger < 65) return "dangerous";
    return "deadly";
}

static const char *AccuracyWord(int32_t accuracy)
{
    if (accuracy >= 85) return "careful";
    if (accuracy >= 65) return "workable";
    return "rough";
}

static void DescribeRoutes(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity, "Roads from here:\n");
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != sim->player.location_id &&
            route->to_id != sim->player.location_id) continue;
        CcId destination_id = route->from_id == sim->player.location_id ?
            route->to_id : route->from_id;
        const CcSettlement *destination = CcSimSettlement(sim, destination_id);
        const CcMap *map = CcSimMapForRoute(sim, route->id, sim->player.id);
        const CcSituation *accepted = CcSimAcceptedSituation(sim);
        bool sponsored_night_passage = route->smuggler_route &&
            accepted != NULL &&
            accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
            route->to_id == accepted->target_id;
        if (route->smuggler_route && map == NULL &&
            !sponsored_night_passage) continue;
        Append(output, capacity, "  %d. %s — %d days, %s",
               i + 1, destination != NULL ? destination->name : "unknown",
               route->travel_days, route->closed ? "restricted and tolled" : "open");
        if (map != NULL) {
            Append(output, capacity,
                   ", %s chart says %s (%d days old)%s\n",
                   AccuracyWord(map->accuracy), DangerWord(map->recorded_danger),
                   sim->current_day - map->surveyed_day,
                   route->smuggler_route ? ", hidden road" : "");
        } else if (sponsored_night_passage) {
            Append(output, capacity,
                   ", the sponsor's guide knows this hidden road\n");
        } else {
            Append(output, capacity,
                   ", uncharted: travel is slower and the risk is unknown\n");
        }
    }
    Append(output, capacity, "Use 'travel NUMBER'.\n");
}

static void DescribeMaps(const CcMetagame *metagame,
                         char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Physical map case (%d/%d); catalogue %d/%d:\n",
           CcPlayerMapCount(sim), sim->player.map_capacity,
           CcPlayerMapCollectionCount(sim), CC_MAP_COLLECTION_COUNT);
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->owner_id == sim->player.id &&
            CcSimMapIsCatalogued(sim, map)) {
            Append(output, capacity, "  %d. %s [%s]\n", i + 1,
                   map->name, CcSimMapIsArchived(sim, map) ?
                   "Gloamgate archive" : "carried");
        } else if (map->owner_id == sim->player.location_id) {
            Append(output, capacity, "  %d. %s [for sale: %d crowns]%s\n",
                   i + 1, map->name, map->ask_price,
                   map->contraband ? " [contraband]" : "");
        }
    }
}

static void DescribeCargo(const CcMetagame *metagame,
                          char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    Append(output, capacity,
           "Carriage: %d/%d cargo, %" PRId64 " crowns, reputation %d\n",
           CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity,
           sim->player.coins, sim->player.reputation);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        Append(output, capacity, "  %s: %d carried",
               CcGoodName((CcGood)good), sim->player.cargo[good]);
        if (place != NULL) {
            Append(output, capacity,
                   "; buy %d, sell %d, market stock %d",
                   place->price[good],
                   place->price[good] * 3 / 4 > 0 ?
                       place->price[good] * 3 / 4 : 1,
                   place->stock[good]);
        }
        Append(output, capacity, "\n");
    }
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed &&
            treasure->owner_id == sim->player.id) {
            Append(output, capacity,
                   "  Treasure %d: %s; one slot, value %d crowns\n",
                   i + 1, treasure->name, treasure->appraised_value);
        }
    }
}

static void DescribeTreasures(const CcMetagame *metagame,
                              char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Named treasures (each fills one carriage slot):\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        if (treasure->destroyed) continue;
        const CcSettlement *location = CcSimSettlement(
            sim, treasure->location_id);
        const char *holder = treasure->owner_id == sim->player.id ?
            "carried by you" :
            treasure->owner_id == sim->dragon.id ? "in the dragon hoard" :
            treasure->owner_id == sim->goblins.id ? "held by goblins" :
            location != NULL ? location->name : "lost";
        Append(output, capacity,
               "  %d. %s — Gold %d + Gems %d + work %d; value %d; %s%s\n",
               i + 1, treasure->name, treasure->gold_content,
               treasure->gem_content, treasure->craft_work,
               treasure->appraised_value, holder,
               treasure->owner_id == sim->player.location_id ?
                   " [for sale here]" : "");
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  None have been made yet.\n");
}

static void DescribeDragon(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *lair = CcSimSettlement(
        sim, sim->dragon.lair_settlement_id);
    if (sim->dragon.slain) {
        Append(output, capacity,
               "%s was slain on day %d above %s. Its cave now holds %" PRId64
               " crowns; the goblin cult has broken into hungry raiders. "
               "The Afterdragon's shadow is %d/100, with %d visible egg%s.\n",
               sim->dragon.name, sim->dragon.slain_day,
               lair != NULL ? lair->name : "an unknown cave",
               sim->dragon.hoard, sim->dragon.regional_influence,
               sim->dragon.egg_count,
               sim->dragon.egg_count == 1 ? "" : "s");
    } else {
        Append(output, capacity,
               "%s is a %s above %s, now %s. Its hoard holds %" PRId64
               " crowns, %d Raw Gold, %d Gems, and %d named treasures.\n",
               sim->dragon.name,
               CcDragonLifeStageName(sim->dragon.life_stage),
               lair != NULL ? lair->name : "an unknown cave",
               CcDragonActivityName(sim->dragon.activity),
               sim->dragon.hoard, sim->dragon.hoard_goods[CC_GOOD_GOLD],
               sim->dragon.hoard_goods[CC_GOOD_GEMS],
               CcSimTreasureCountForOwner(sim, sim->dragon.id));
        Append(output, capacity,
               "Crown strength %d/100, body %d/100, memory %d/100, territory %d/100, dragon shadow %d/100; age %d years.\n",
               sim->dragon.crown_strength, sim->dragon.body_condition,
               sim->dragon.memory_integrity,
               sim->dragon.territory_stability,
               sim->dragon.regional_influence,
               sim->dragon.age_days / 365);
        if (sim->dragon.egg_count > 0) {
            Append(output, capacity,
                   "%d egg%s lie in the brood hoard. Goblin Ashkeepers have %d days of tending left.\n",
                   sim->dragon.egg_count,
                   sim->dragon.egg_count == 1 ? "" : "s",
                   sim->dragon.brood_days_remaining);
        }
    }
    if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND) {
        const CcSettlement *target = CcSimSettlement(
            sim, sim->goblins.tribute_target_id);
        Append(output, capacity,
               "%s is travelling from its lair to raid %s.\n",
               sim->goblins.name,
               target != NULL ? target->name : "a rich town");
    } else if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING) {
        const CcSettlement *origin = CcSimSettlement(
            sim, sim->goblins.tribute_target_id);
        Append(output, capacity,
               "%s is carrying physical loot from %s back to its lair.\n",
               sim->goblins.name,
               origin != NULL ? origin->name : "a rich town");
    } else if (sim->goblins.tribute_phase ==
               CC_GOBLIN_TRIBUTE_TO_DRAGON) {
        Append(output, capacity,
               "%s is carrying %" PRId64
               " crowns, %d Raw Gold, %d Gems, and %s toward the cave.\n",
               sim->goblins.name, sim->goblins.carried_tribute,
               sim->goblins.carried_goods[CC_GOOD_GOLD],
               sim->goblins.carried_goods[CC_GOOD_GEMS],
               sim->goblins.carried_treasure_id != 0U ?
                   "a named treasure" : "no named treasure");
    } else {
        Append(output, capacity,
               "%s waits in its lair with %d Food, %d Tools, %d Weapons, and %" PRId64
               " crowns. It has defended the dragon's hoard %d times; its Hoardkeepers, Ashkeepers, Tongues, and Foragers follow real needs.\n",
               sim->goblins.name,
               sim->goblins.lair_stock[CC_GOOD_FOOD],
               sim->goblins.lair_stock[CC_GOOD_TOOLS],
               sim->goblins.lair_stock[CC_GOOD_WEAPONS],
               sim->goblins.lair_coins, sim->goblins.hoard_defenses);
    }
    if (sim->hoard_raiders.phase == CC_HOARD_RAIDERS_OUTBOUND) {
        const CcSettlement *origin = CcSimSettlement(
            sim, sim->hoard_raiders.origin_settlement_id);
        if (sim->hoard_raiders.motive == CC_HOARD_RAID_WAR_FINANCE) {
            Append(output, capacity,
                   "%s is travelling from %s to rob the hoard; its supply crisis is %d.\n",
                   sim->hoard_raiders.name,
                   origin != NULL ? origin->name : "a frontier keep",
                   origin != NULL ?
                       CcSimWarSupplyCrisisAtSettlement(sim, origin->id) : 0);
        } else {
            Append(output, capacity,
                   "%s is travelling from %s to rob the hoard after inequality or monastery default reached %d.\n",
                   sim->hoard_raiders.name,
                   origin != NULL ? origin->name : "an unequal town",
                   origin != NULL ?
                       CcSimInequalityAtSettlement(sim, origin->id) : 0);
        }
    } else if (sim->hoard_raiders.phase == CC_HOARD_RAIDERS_RETURNING) {
        const CcSettlement *origin = CcSimSettlement(
            sim, sim->hoard_raiders.origin_settlement_id);
        Append(output, capacity,
               "%s carries %" PRId64 " stolen crowns home to %s for %s.\n",
               sim->hoard_raiders.name,
               sim->hoard_raiders.carried_treasure,
               origin != NULL ? origin->name : "an unknown town",
               sim->hoard_raiders.motive == CC_HOARD_RAID_WAR_FINANCE ?
                   "the war chest" : "bread and debts");
    }
    if (sim->dragon_campaign.phase == CC_DRAGON_CAMPAIGN_OUTBOUND) {
        const CcSettlement *origin = CcSimSettlement(
            sim, sim->dragon_campaign.origin_settlement_id);
        Append(output, capacity,
               "An allied host marches from %s with %d Food, %d Tools, and %d Weapons; battle is %d days away.\n",
               origin != NULL ? origin->name : "a mustering ground",
               sim->dragon_campaign.supplies[CC_GOOD_FOOD],
               sim->dragon_campaign.supplies[CC_GOOD_TOOLS],
               sim->dragon_campaign.supplies[CC_GOOD_WEAPONS],
               sim->dragon_campaign.days_remaining);
    } else if (sim->dragon_campaign.phase == CC_DRAGON_CAMPAIGN_RETURNING) {
        Append(output, capacity,
               "The dragon host is %d days from home with %" PRId64
               " recovered crowns and the cave goods in its train.\n",
               sim->dragon_campaign.days_remaining,
               sim->dragon_campaign.recovered_coins);
    }
    if (sim->dragon.stolen_outstanding > 0) {
        const CcSettlement *target = CcSimSettlement(
            sim, sim->dragon.retaliation_target_id);
        const CcTreasure *stolen = sim->dragon.stolen_treasure_id != 0U ?
            CcSimTreasure(sim, sim->dragon.stolen_treasure_id) : NULL;
        if (stolen != NULL) {
            Append(output, capacity,
                   "%s remains outside its remembered place. Omens gather over %s; old readers count %d nights. Only the exact object will close this wound.\n",
                   stolen->name,
                   target != NULL ? target->name : "the countryside",
                   sim->dragon.omen_days_remaining);
        } else {
            Append(output, capacity,
                   "%" PRId64 " stolen crowns remain missing. Omens gather over %s; old readers count %d nights.\n",
                   sim->dragon.stolen_outstanding,
                   target != NULL ? target->name : "the countryside",
                   sim->dragon.omen_days_remaining);
        }
    } else if (!sim->dragon.slain) {
        Append(output, capacity,
               "The dragon is calm. Goblins raid for food, gear, and offerings; only theft from the delivered hoard brings dragon fire.\n");
    } else {
        Append(output, capacity,
               "No dragon remains to retaliate. Goblins still raid when their lair runs short of food, tools, or weapons.\n");
    }
    if (!sim->dragon.slain &&
        sim->player.location_id == sim->dragon.lair_settlement_id) {
        Append(output, capacity,
               "Here you may steal or return crowns, or use 'dragon steal-treasure NUMBER' and 'dragon return-treasure' for a named object.\n");
    }
}

static const char *InequalityWord(int32_t score)
{
    if (score < 40) return "shared";
    if (score < 60) return "strained";
    if (score < 75) return "bitter";
    return "explosive";
}

static void DescribeInequality(const CcMetagame *metagame,
                               char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Social fault lines (hunger beside stored coin, high prices, and weak commoners):\n");
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        int32_t score = CcSimInequalityAtSettlement(sim, place->id);
        Append(output, capacity,
               "  %s: %d, %s — prosperity %d, hunger %d, food %d crowns, market holds %" PRId64 ", war burden %d\n",
               place->name, score, InequalityWord(score),
               place->prosperity, place->hunger,
               place->price[CC_GOOD_FOOD], place->market_coins,
               CcSimWarBurdenAtSettlement(sim, place->id));
    }
    Append(output, capacity,
           "At 75 or more, with real hunger, people may risk a raid on the dragon hoard. A realm near its monastery credit limit can also drive the Ash-Poor to the cave.\n");
}

static void DescribeEconomy(const CcMetagame *metagame,
                            char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Material economy (goods only appear at a source or through a recipe):\n");
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        Append(output, capacity,
               "  %s: Food %d, Iron %d, Tools %d, Weapons %d, Gold %d, Gems %d",
               place->name, place->stock[CC_GOOD_FOOD],
               place->stock[CC_GOOD_IRON], place->stock[CC_GOOD_TOOLS],
               place->stock[CC_GOOD_WEAPONS], place->stock[CC_GOOD_GOLD],
               place->stock[CC_GOOD_GEMS]);
        if (CcSettlementHasService(place, CC_SERVICE_FARM)) {
            Append(output, capacity, "; fields %d%%", place->field_yield);
        }
        if (CcSettlementHasService(place, CC_SERVICE_MINE)) {
            Append(output, capacity,
                   "; mountain Iron %d, Gold %s %d/12, Gems %s %d/48",
                   place->iron_deposit, place->gold_seam ? "seam" : "none",
                   place->gold_progress, place->gem_seam ? "seam" : "none",
                   place->gem_progress);
        }
        Append(output, capacity, "; treasures %d\n",
               CcSimTreasureCountForOwner(sim, place->id));
    }
    Append(output, capacity,
           "A smith spends 2 Iron per Tools bundle and 3 Iron per Weapons bundle. A market or capital smith may spend 1 Raw Gold, 1 Gem, and 3 weeks of work on one named one-slot treasure.\n");
    Append(output, capacity,
           "The copied monastery ledger holds %" PRId64 " crowns. It lends real deposited coin for famine grain and productive Tools; realms repay from treasury and market tithes.\n",
           sim->iron_ledger_reserve);
}

static void DescribeWar(const CcMetagame *metagame,
                        char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Kings declare war, offer peace, and make dragon alliances by physical courier. Nothing changes until the message arrives; a lost or corrupted dispatch may delay or twist the result. Restricted roads still carry reduced freight under tolls and sanctions.\n");
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        Append(output, capacity,
               "  %s: treasury %" PRId64 ", monastery debt %" PRId64
               ", legitimacy %d\n",
               kingdom->name, kingdom->treasury,
               kingdom->iron_ledger_debt, kingdom->legitimacy);
    }
    Append(output, capacity, "Confirmed relations:\n");
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            Append(output, capacity, "  %s / %s: %s since day %d\n",
                   sim->kingdoms[first].name,
                   sim->kingdoms[second].name,
                   CcDiplomaticStateName(sim->diplomacy[first][second]),
                   sim->diplomacy_changed_day[first][second]);
        }
    }
    Append(output, capacity, "Dispatches on the roads:\n");
    int32_t dispatches = 0;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        const CcCourier *courier = &sim->couriers[i];
        if (courier->status == CC_COURIER_DELIVERED ||
            courier->status == CC_COURIER_LOST ||
            courier->status == CC_COURIER_DISTORTED) continue;
        const CcSettlement *from = CcSimSettlement(
            sim, courier->current_settlement_id);
        const CcSettlement *to = CcSimSettlement(
            sim, courier->destination_settlement_id);
        Append(output, capacity,
               "  %s: %s to %s, reliability %d%%%s\n",
               CourierPurpose(courier->kind),
               from != NULL ? from->name : "the road",
               to != NULL ? to->name : "an unknown court",
               courier->reliability,
               courier->status == CC_COURIER_WITH_PLAYER ?
                   " [in your carriage]" : "");
        dispatches += 1;
    }
    if (dispatches == 0) Append(output, capacity, "  None.\n");
    Append(output, capacity, "Frontier roads:\n");
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (!CcSimRouteCrossesWarBorder(sim, route->id)) continue;
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        Append(output, capacity,
               "  %s to %s: %s, security %d; burdens %d/%d\n",
               from != NULL ? from->name : "unknown",
               to != NULL ? to->name : "unknown",
               route->closed ? "restricted/tolled" : "open", route->security,
               from != NULL ?
                   CcSimWarBurdenAtSettlement(sim, from->id) : 0,
               to != NULL ? CcSimWarBurdenAtSettlement(sim, to->id) : 0);
    }
    Append(output, capacity,
           "Garrison ledgers:\n");
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->function != CC_SETTLEMENT_FORTRESS &&
            place->function != CC_SETTLEMENT_CAPITAL) continue;
        Append(output, capacity,
               "  %s: food %d, tools %d, weapons %d, market %" PRId64
               ", war chest %" PRId64 ", supply crisis %d\n",
               place->name, place->stock[CC_GOOD_FOOD],
               place->stock[CC_GOOD_TOOLS],
               place->stock[CC_GOOD_WEAPONS], place->market_coins,
               place->war_chest,
               CcSimWarSupplyCrisisAtSettlement(sim, place->id));
    }
    Append(output, capacity,
           "The crown moves existing treasury gold into each war chest. The chest pays wages and buys real food, tools, and weapons from named towns; those goods travel as shipments. Weapons stay equipped until lost in fighting. A Crown Levy requires burden 35, supply crisis 55, less than 200 liquid war crowns, and legitimacy below 65.\n");
}

static void DescribeStatus(const CcMetagame *metagame,
                           char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcSettlement *place = CurrentPlace(metagame);
    Append(output, capacity,
           "Day %d at %s. Food %d/%d, price %d, hunger %d, security %d.\n",
           sim->current_day, place != NULL ? place->name : "the road",
           place != NULL ? place->stock[CC_GOOD_FOOD] : 0,
           place != NULL ? place->reserve_target[CC_GOOD_FOOD] : 0,
           place != NULL ? place->price[CC_GOOD_FOOD] : 0,
           place != NULL ? place->hunger : 0,
           place != NULL ? place->security : 0);
    DescribeCargo(metagame, output, capacity);
}

static void DescribeHistory(const CcMetagame *metagame, int32_t count,
                            char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    int32_t shown = count < sim->event_count ? count : sim->event_count;
    Append(output, capacity, "Recent consequences:\n");
    for (int32_t i = shown - 1; i >= 0; --i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL) {
            Append(output, capacity, "  day %d: %s\n", event->day,
                   event->text);
        }
    }
}

static void DescribeDebrief(const CcMetagame *metagame,
                            char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    int32_t resolved = 0;
    int32_t failed = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_RESOLVED) resolved += 1;
        if (sim->situations[i].status == CC_SITUATION_FAILED) failed += 1;
    }
    Append(output, capacity,
           "PLAYTEST DEBRIEF — day %d, %d resolved commitments, %d failed\n"
           "Answer from memory, before reading history:\n"
           "  1. Tell the story of what happened.\n"
           "  2. Which choice felt hardest, and why?\n"
           "  3. What surprised you later?\n"
           "  4. Who do you now trust or distrust?\n"
           "  5. What would you do next?\n",
           sim->current_day, resolved, failed);
}

static void DescribeHelp(char *output, size_t capacity)
{
    Append(output, capacity,
           "See the world:\n"
           "  look, causes, people, rumors, charters, routes, maps, cargo, economy, treasures, inequality, war, dragon, status, history [COUNT]\n"
           "Make commitments:\n"
           "  accept NUMBER, refuse NUMBER, abandon\n"
           "Move goods and people:\n"
           "  buy food|iron|tools|weapons|gold|gems COUNT\n"
           "  sell food|iron|tools|weapons|gold|gems COUNT\n"
           "  buy-map NUMBER, sell-map NUMBER\n"
           "  archive-map NUMBER, retrieve-map NUMBER (in Gloamgate)\n"
           "  buy-treasure NUMBER, sell-treasure NUMBER, travel NUMBER\n"
           "Act on the road and world:\n"
           "  road fight|bargain, repair NUMBER tools|cash\n"
           "  dungeon public|smuggler|seal, wait DAYS\n"
           "  dragon steal COUNT, dragon return COUNT (at the cave)\n"
           "  dragon steal-treasure NUMBER, dragon return-treasure\n"
           "Keep the test:\n"
           "  save PATH, load PATH, debrief, quit\n");
}

static void FinishTravel(CcMetagame *metagame,
                         char *output, size_t capacity)
{
    CcSim *sim = &metagame->sim;
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
    if (sim->journey.active &&
        sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
        const CcEvent *event = CcSimRecentEvent(sim, 0);
        Append(output, capacity, "%s\n",
               event != NULL ? event->text : "The road is blocked.");
        Append(output, capacity,
               "Choose 'road fight' or 'road bargain'.\n");
    } else {
        const CcSettlement *place = CurrentPlace(metagame);
        Append(output, capacity, "The carriage arrives at %s on day %d.\n",
               place != NULL ? place->name : "an unknown place",
               sim->current_day);
    }
}

static bool ApplyCommand(CcMetagame *metagame, const CcCommand *command,
                         char *output, size_t capacity)
{
    char error[192];
    if (!CcSimApply(&metagame->sim, command, error, sizeof(error))) {
        Append(output, capacity, "%s\n", error);
        return false;
    }
    return true;
}

static bool ValidateAfterAction(CcMetagame *metagame,
                                char *output, size_t capacity)
{
    char error[192];
    if (CcSimValidate(&metagame->sim, error, sizeof(error))) return true;
    Append(output, capacity, "Internal simulation error: %s\n", error);
    return false;
}

void CcMetagameInit(CcMetagame *metagame, uint32_t seed)
{
    if (metagame == NULL) return;
    *metagame = (CcMetagame){0};
    CcSimInit(&metagame->sim, seed);
    metagame->sim.player.location_id = metagame->sim.settlements[1].id;
    metagame->sim.carriage.location_id = metagame->sim.player.location_id;
    metagame->sim.player.coins = 75;
}

void CcMetagameIntro(const CcMetagame *metagame,
                     char *output, size_t output_capacity)
{
    if (output == NULL || output_capacity == 0U) return;
    output[0] = '\0';
    Append(output, output_capacity,
           "CROWNLESS CARRIAGE — THE EMPTY GRANARY\n"
           "You decide what people, goods, and news fit in one carriage.\n"
           "The world will continue after every promise and refusal.\n"
           "You start at the hungry crossroads with 75 crowns and no given plan.\n");
    DescribeLook(metagame, output, output_capacity);
}

bool CcMetagameExecute(CcMetagame *metagame, const char *line,
                       char *output, size_t output_capacity)
{
    if (metagame == NULL || line == NULL || output == NULL ||
        output_capacity == 0U) return false;
    output[0] = '\0';
    char copy[256];
    (void)snprintf(copy, sizeof(copy), "%s", line);
    size_t length = strlen(copy);
    while (length > 0U &&
           (copy[length - 1U] == '\n' || copy[length - 1U] == '\r')) {
        copy[--length] = '\0';
    }
    char *command = strtok(copy, " \t");
    char *first = strtok(NULL, " \t");
    char *second = strtok(NULL, " \t");
    if (command == NULL) return true;

    if (strcmp(command, "help") == 0) DescribeHelp(output, output_capacity);
    else if (strcmp(command, "look") == 0) {
        DescribeLook(metagame, output, output_capacity);
    } else if (strcmp(command, "causes") == 0) {
        DescribeCauses(metagame, output, output_capacity);
    } else if (strcmp(command, "people") == 0) {
        DescribePeople(metagame, output, output_capacity);
    } else if (strcmp(command, "rumors") == 0) {
        DescribeRumors(metagame, output, output_capacity);
    } else if (strcmp(command, "charters") == 0) {
        DescribeCharters(metagame, output, output_capacity);
    } else if (strcmp(command, "routes") == 0) {
        DescribeRoutes(metagame, output, output_capacity);
    } else if (strcmp(command, "maps") == 0) {
        DescribeMaps(metagame, output, output_capacity);
    } else if (strcmp(command, "cargo") == 0) {
        DescribeCargo(metagame, output, output_capacity);
    } else if (strcmp(command, "economy") == 0) {
        DescribeEconomy(metagame, output, output_capacity);
    } else if (strcmp(command, "treasures") == 0) {
        DescribeTreasures(metagame, output, output_capacity);
    } else if (strcmp(command, "inequality") == 0) {
        DescribeInequality(metagame, output, output_capacity);
    } else if (strcmp(command, "war") == 0) {
        DescribeWar(metagame, output, output_capacity);
    } else if (strcmp(command, "dragon") == 0 && first == NULL) {
        DescribeDragon(metagame, output, output_capacity);
    } else if (strcmp(command, "dragon") == 0) {
        int32_t amount = 0;
        int32_t index = 0;
        CcCommand action = {0};
        if (strcmp(first, "steal") == 0 && ParseAmount(second, &amount)) {
            action.kind = CC_COMMAND_STEAL_DRAGON_HOARD;
        } else if (strcmp(first, "return") == 0 &&
                   ParseAmount(second, &amount)) {
            action.kind = CC_COMMAND_RETURN_DRAGON_TREASURE;
        } else if (strcmp(first, "steal-treasure") == 0 &&
                   ParseIndex(second, metagame->sim.treasure_count, &index)) {
            action.kind = CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE;
            action.target_id = metagame->sim.treasures[index].id;
        } else if (strcmp(first, "return-treasure") == 0 && second == NULL &&
                   metagame->sim.dragon.stolen_treasure_id != 0U) {
            action.kind = CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE;
            action.target_id = metagame->sim.dragon.stolen_treasure_id;
        } else {
            Append(output, output_capacity,
                   "Use 'dragon steal COUNT', 'dragon return COUNT', 'dragon steal-treasure NUMBER', or 'dragon return-treasure'.\n");
            return false;
        }
        action.amount = amount;
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        DescribeDragon(metagame, output, output_capacity);
    } else if (strcmp(command, "status") == 0) {
        DescribeStatus(metagame, output, output_capacity);
    } else if (strcmp(command, "history") == 0) {
        int32_t count = 8;
        if (first != NULL && (!ParseDays(first, &count) || count > 20)) {
            Append(output, output_capacity, "History count must be 1 to 20.\n");
            return false;
        }
        DescribeHistory(metagame, count, output, output_capacity);
    } else if (strcmp(command, "debrief") == 0) {
        DescribeDebrief(metagame, output, output_capacity);
    } else if (strcmp(command, "accept") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.situation_count, &index)) {
            Append(output, output_capacity, "Choose a charter number.\n");
            return false;
        }
        CcCommand action = {
            .kind = CC_COMMAND_ACCEPT_SITUATION,
            .target_id = metagame->sim.situations[index].id
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity, "Promise accepted. Cargo space and time now have a purpose.\n");
    } else if (strcmp(command, "abandon") == 0) {
        CcCommand action = {.kind = CC_COMMAND_ABANDON_SITUATION};
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity, "The promise is withdrawn. The need remains.\n");
    } else if (strcmp(command, "refuse") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.situation_count, &index)) {
            Append(output, output_capacity, "Choose an offer number.\n");
            return false;
        }
        CcCommand action = {
            .kind = CC_COMMAND_REFUSE_SITUATION,
            .target_id = metagame->sim.situations[index].id
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The refusal is spoken aloud. That sponsor will remember it.\n");
    } else if (strcmp(command, "buy") == 0 ||
               strcmp(command, "sell") == 0) {
        CcGood good;
        int32_t amount;
        if (!ParseGood(first, &good) || !ParseDays(second, &amount) ||
            amount > CC_CARGO_CAPACITY * 8) {
            Append(output, output_capacity,
                   "Use 'buy food 8' or 'sell weapons 2'; count must fit the carriage.\n");
            return false;
        }
        if (strcmp(command, "sell") == 0) amount = -amount;
        CcCommand action = {
            .kind = CC_COMMAND_TRADE,
            .good = good,
            .amount = amount
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity, "%s %d %s.\n",
               amount > 0 ? "Loaded" : "Unloaded",
               amount > 0 ? amount : -amount, CcGoodName(good));
    } else if (strcmp(command, "buy-map") == 0 ||
               strcmp(command, "sell-map") == 0 ||
               strcmp(command, "archive-map") == 0 ||
               strcmp(command, "retrieve-map") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.map_count, &index)) {
            Append(output, output_capacity, "Choose a map number.\n");
            return false;
        }
        CcCommandKind kind = strcmp(command, "buy-map") == 0 ?
            CC_COMMAND_BUY_MAP : strcmp(command, "sell-map") == 0 ?
            CC_COMMAND_SELL_MAP : strcmp(command, "archive-map") == 0 ?
            CC_COMMAND_ARCHIVE_MAP : CC_COMMAND_RETRIEVE_MAP;
        CcCommand action = {
            .kind = kind,
            .target_id = metagame->sim.maps[index].id
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The physical map collection is rearranged.\n");
    } else if (strcmp(command, "buy-treasure") == 0 ||
               strcmp(command, "sell-treasure") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.treasure_count, &index)) {
            Append(output, output_capacity,
                   "Choose a named treasure number from 'treasures'.\n");
            return false;
        }
        CcCommand action = {
            .kind = strcmp(command, "buy-treasure") == 0 ?
                CC_COMMAND_BUY_TREASURE : CC_COMMAND_SELL_TREASURE,
            .target_id = metagame->sim.treasures[index].id
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The named treasure changes hands and the cargo ledger changes by one slot.\n");
    } else if (strcmp(command, "travel") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.route_count, &index)) {
            Append(output, output_capacity, "Choose a road number.\n");
            return false;
        }
        const CcRoute *route = &metagame->sim.routes[index];
        CcId destination = route->from_id == metagame->sim.player.location_id ?
            route->to_id : route->to_id == metagame->sim.player.location_id ?
            route->from_id : 0U;
        if (destination == 0U) {
            Append(output, output_capacity, "That road does not leave this place.\n");
            return false;
        }
        CcCommand action = {
            .kind = CC_COMMAND_TRAVEL,
            .target_id = destination
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        FinishTravel(metagame, output, output_capacity);
    } else if (strcmp(command, "road") == 0) {
        int32_t condition_before = metagame->sim.carriage.condition;
        CcMoney coins_before = metagame->sim.player.coins;
        CcCommand action = {0};
        if (first != NULL && strcmp(first, "fight") == 0) {
            action.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT;
        } else if (first != NULL && strcmp(first, "bargain") == 0) {
            action.kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE;
        } else {
            Append(output, output_capacity, "Choose 'road fight' or 'road bargain'.\n");
            return false;
        }
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The road choice costs %" PRId64
               " crowns and %d carriage condition.\n",
               coins_before - metagame->sim.player.coins,
               condition_before - metagame->sim.carriage.condition);
        FinishTravel(metagame, output, output_capacity);
    } else if (strcmp(command, "repair") == 0) {
        int32_t index;
        int32_t method = second != NULL && strcmp(second, "tools") == 0 ? 1 :
                         second != NULL && strcmp(second, "cash") == 0 ? 2 : 0;
        if (!ParseIndex(first, metagame->sim.route_count, &index) ||
            method == 0) {
            Append(output, output_capacity,
                   "Use 'repair NUMBER tools' or 'repair NUMBER cash'.\n");
            return false;
        }
        CcCommand action = {
            .kind = CC_COMMAND_REPAIR_ROUTE,
            .target_id = metagame->sim.routes[index].id,
            .amount = method
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The treaty bridge reopens. Other shipments can now follow.\n");
    } else if (strcmp(command, "dungeon") == 0) {
        CcDungeonState state;
        if (first != NULL && strcmp(first, "public") == 0) {
            state = CC_DUNGEON_PUBLIC_ROUTE;
        } else if (first != NULL && strcmp(first, "smuggler") == 0) {
            state = CC_DUNGEON_SMUGGLER_ROUTE;
        } else if (first != NULL && strcmp(first, "seal") == 0) {
            state = CC_DUNGEON_RESEALED;
        } else {
            Append(output, output_capacity,
                   "Choose 'dungeon public', 'dungeon smuggler', or 'dungeon seal'.\n");
            return false;
        }
        if (metagame->sim.dungeon_count < 1) return false;
        CcCommand action = {
            .kind = CC_COMMAND_CHANGE_DUNGEON,
            .target_id = metagame->sim.dungeons[0].id,
            .dungeon_state = state
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        Append(output, output_capacity,
               "The mine route is now %s. Its trade, rulers, and monsters have all changed.\n",
               CcDungeonStateName(state));
    } else if (strcmp(command, "wait") == 0) {
        int32_t days;
        if (!ParseDays(first, &days)) {
            Append(output, output_capacity, "Wait between 1 and 365 days.\n");
            return false;
        }
        if (metagame->sim.journey.active) {
            Append(output, output_capacity, "Resolve the road before waiting.\n");
            return false;
        }
        CcSimAdvanceDays(&metagame->sim, days);
        Append(output, output_capacity,
               "%d days pass. Markets, factions, roads, and threats continue without you.\n",
               days);
        DescribeLook(metagame, output, output_capacity);
    } else if (strcmp(command, "save") == 0) {
        char error[192];
        if (first == NULL || !CcSaveWrite(first, &metagame->sim,
                                          error, sizeof(error))) {
            Append(output, output_capacity, "%s\n",
                   first == NULL ? "Give the save a path." : error);
            return false;
        }
        Append(output, output_capacity, "Campaign saved to %s.\n", first);
    } else if (strcmp(command, "load") == 0) {
        char error[192];
        CcSim loaded;
        if (first == NULL || !CcSaveRead(first, &loaded,
                                         error, sizeof(error))) {
            Append(output, output_capacity, "%s\n",
                   first == NULL ? "Give the save a path." : error);
            return false;
        }
        metagame->sim = loaded;
        Append(output, output_capacity, "Campaign loaded from %s.\n", first);
        DescribeLook(metagame, output, output_capacity);
    } else if (strcmp(command, "quit") == 0) {
        metagame->quit_requested = true;
        Append(output, output_capacity,
               "The carriage stops here. The world would continue.\n");
        return true;
    } else {
        Append(output, output_capacity,
               "Unknown action. Type 'help' for the metagame commands.\n");
        return false;
    }
    return ValidateAfterAction(metagame, output, output_capacity);
}
