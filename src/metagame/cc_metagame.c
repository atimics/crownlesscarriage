#include "metagame/cc_metagame.h"

#include "persistence/cc_save.h"
#include "story/cc_story.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ApplyCommand(CcMetagame *metagame, const CcCommand *command,
                         char *output, size_t capacity);

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
    return "a private letter";
}

static const char *KingdomPressureName(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD: return "road strain";
        case CC_KINGDOM_CALLING_IRON: return "garrison strain";
        case CC_KINGDOM_CALLING_DEEP: return "deep strain";
        case CC_KINGDOM_CALLING_COUNT: break;
    }
    return "realm strain";
}

static const char *KingdomPressureWord(int32_t pressure)
{
    if (pressure < 30) return "steady";
    if (pressure < 50) return "watchful";
    if (pressure < 70) return "strained";
    if (pressure < 85) return "severe";
    return "breaking";
}

static const char *KingdomPower(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD:
            return "food, markets, maps, and carriage roads";
        case CC_KINGDOM_CALLING_IRON:
            return "mines, smiths, fortifications, and armed force";
        case CC_KINGDOM_CALLING_DEEP:
            return "capital wealth, old institutions, and dungeon access";
        case CC_KINGDOM_CALLING_COUNT: break;
    }
    return "uncertain claims";
}

static const char *KingdomDependence(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD:
            return "open roads and buyers beyond its borders";
        case CC_KINGDOM_CALLING_IRON:
            return "imported food and a paid, supplied garrison";
        case CC_KINGDOM_CALLING_DEEP:
            return "contained ruins, safe pilgrim roads, and frontier labour";
        case CC_KINGDOM_CALLING_COUNT: break;
    }
    return "the wider road network";
}

static const char *KingdomContradiction(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD:
            return "It needs peace to prosper, yet can profit from carrying a war.";
        case CC_KINGDOM_CALLING_IRON:
            return "It holds the tools of conquest, yet cannot feed those who wield them.";
        case CC_KINGDOM_CALLING_DEEP:
            return "Its public order depends on wealth drawn from places it cannot fully control.";
        case CC_KINGDOM_CALLING_COUNT: break;
    }
    return "Its claim is larger than its reach.";
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

static void MineObjective(const CcSituation *situation,
                          const CcCharacter *speaker,
                          char *buffer, size_t capacity)
{
    const char *name = speaker != NULL ? speaker->name : "the miner";
    if (situation->discovery_stage == CC_DISCOVERY_OFFER) {
        (void)snprintf(buffer, capacity,
                       "Find Cera in the west gallery.");
        return;
    }
    (void)snprintf(buffer, capacity, "Talk to %s.", name);
}

static bool IsNamedSettlement(const CcSim *sim,
                              const CcSettlement *place, int32_t slot)
{
    return sim != NULL && place != NULL && slot >= 0 &&
        slot < sim->settlement_count && place->id == sim->settlements[slot].id;
}

static const char *StoryRoadName(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return "the unknown road";
    if (route->id == sim->routes[1].id ||
        route->id == sim->routes[2].id) return "the King's Road";
    if (route->id == sim->routes[6].id) return "the Night Road";
    if (route->id == sim->routes[0].id) return "the Baker's Road";
    if (route->id == sim->routes[3].id) return "the Silver Road";
    if (route->id == sim->routes[4].id) return "the Pilgrim Road";
    if (route->id == sim->routes[5].id) return "the Barrow Road";
    return "the Crown Road";
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
    Append(output, capacity, "\n%s, day %d\n", place->name,
           sim->current_day);
    if (IsNamedSettlement(sim, place, 0)) {
        Append(output, capacity,
               "Thornford's chimneys smoke above the hill granaries.\n"
               "Beside the bridge, a mossy milestone bears three little crowns and one nearly-rubbed-away wheel. You arrived on foot.\n");
    } else if (IsNamedSettlement(sim, place, 1)) {
        Append(output, capacity,
               "Every lane in Gloamgate enters the round market and leaves by another gate. No two map sellers agree where the lanes go.\n"
               "At the fountain, a stone queen has lost her head. Someone has put a wooden bowl where her crown should be, and scratched a tiny wheel beneath it.\n");
    } else if (IsNamedSettlement(sim, place, 2)) {
        Append(output, capacity,
               "Alderwatch stands over the ravine in iron, red cloth, and strict handwriting. The bridge is sound. A chain, polished by many gloves, keeps its gate shut.\n");
    } else if (IsNamedSettlement(sim, place, 3)) {
        Append(output, capacity,
               "Silverwick climbs the hill in black stone steps. Its chimneys cough silver dust, and the market clock has stopped at breakfast.\n"
               "Children wait at the public ovens with empty tins. Below them, somewhere under the street, the mine takes a long cold breath.\n");
    } else if (IsNamedSettlement(sim, place, 4)) {
        Append(output, capacity,
               "Rosespire's roofs rise like red petals around the palace. Servants use the small doors. Important people prefer doors tall enough for banners.\n");
    } else if (IsNamedSettlement(sim, place, 5)) {
        Append(output, capacity,
               "Hollowbarrow is built where the road ends and the old hill begins. Little offerings hang from the eaves, though nobody agrees who they are meant to please.\n");
    }
    if (place->stock[CC_GOOD_FOOD] < place->reserve_target[CC_GOOD_FOOD] / 2) {
        Append(output, capacity,
               "The baker has flour in his hair but none on his apron. Empty baskets wait outside the guarded store.\n");
    } else if (place->stock[CC_GOOD_FOOD] <
               place->reserve_target[CC_GOOD_FOOD]) {
        Append(output, capacity,
               "Bread is sold today, in loaves small enough to make the price look embarrassed.\n");
    } else {
        Append(output, capacity,
               "There is food in the market today. People still glance at the road before buying it.\n");
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
    if (place->id == sim->goblins.lair_settlement_id) {
        Append(output, capacity,
               "Nara Soot-Tongue receives traders beside the Cinder Tithe's store caves.\n");
    }
    if (place->id == sim->goblins.tribute_target_id &&
        sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING) {
        Append(output, capacity,
               "Word has arrived that goblins are mustering for this settlement. There is still time to warn or intercept them.\n");
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
        const CcCharacter *speaker = CcSimSituationConversationCharacter(
            sim, situation, place->id);
        if (speaker != NULL && !CcSimSituationCanAccept(sim, situation)) {
            char objective[128];
            MineObjective(situation, speaker, objective, sizeof(objective));
            Append(output, capacity, "%s\n", objective);
        } else {
            Append(output, capacity,
                   "%s has a job for you at the town board.\n",
                   situation->sponsor_name);
        }
    }
    Append(output, capacity,
           "You may 'talk NUMBER' from the charter list, listen to 'rumors', or inspect the 'roads'.\n"
           "Type 'status' when you want the plain numbers, or 'help' for every action.\n");
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
    if (event_id == 0U && IsNamedSettlement(sim, place, 0)) {
        for (int32_t offset = 0; offset < sim->event_count; ++offset) {
            const CcEvent *event = CcSimRecentEvent(sim, offset);
            if (event != NULL && event->kind == CC_EVENT_HARVEST_FAILED &&
                event->location_id == place->id) {
                event_id = event->id;
                break;
            }
        }
    }
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
    Append(output, capacity, "People who have stopped pretending not to watch the carriage:\n");
    int32_t shown = 0;
    if (IsNamedSettlement(sim, place, 1)) {
        Append(output, capacity,
               "  A map seller guards three disagreeing road notes and claims the wheel scratched on her fountain is 'old carriage nonsense.'\n");
        shown += 1;
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE || place == NULL ||
            CcSimSituationOfferSettlementId(sim, situation) != place->id) continue;
        const CcCharacter *speaker = CcSimSituationConversationCharacter(
            sim, situation, place->id);
        if (speaker == NULL) continue;
        if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION &&
            !CcSimSituationCanAccept(sim, situation)) {
            char objective[128];
            MineObjective(situation, speaker, objective, sizeof(objective));
            Append(output, capacity, "  %s Use 'talk %d'.\n",
                   objective, i + 1);
        } else {
            Append(output, capacity,
                   "  Talk to %s about job %d with 'talk %d'.\n",
                   speaker->name, i + 1, i + 1);
        }
        shown += 1;
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcRoute *route = CcSimRoute(sim, sim->bandits[i].route_id);
        if (place != NULL && route != NULL &&
            (route->from_id == place->id || route->to_id == place->id)) {
            Append(output, capacity,
                   "  People call the old-road camp %s. They collect a road price and remember who paid it.\n",
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
                   "  %s carries unopened orders to fund the border war with the dragon's gold.\n",
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
               "  The miller swears the western fields still whisper with grain. He lowers his voice before saying where the wagons went.\n"
               "  A clerk says the bridge is closed, then admits it opens for the right paper, the right purse, or the wrong reason.\n"
               "  A night driver folds a scrap of paper into a fox. Held over a candle, its pinholes look rather like a road.\n");
    } else if (place != NULL && place->id == sim->settlements[0].id) {
        Append(output, capacity,
               "  The harvest failed. People are watching the eastern road for food.\n"
               "  The treaty bridge is closed, and the relief wagons are late.\n");
    } else if (place != NULL && place->id == sim->settlements[3].id) {
        Append(output, capacity,
               "  Bren Alder ran out of the west gallery and left his lamp behind. Jory is looking for him.\n");
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
    Append(output, capacity, "Current objectives:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            (CcSimSituationOfferSettlementId(sim, situation) !=
                 sim->player.location_id &&
             (accepted == NULL || accepted->id != situation->id))) continue;
        const CcCharacter *speaker = CcSimSituationConversationCharacter(
            sim, situation, sim->player.location_id);
        char target[96];
        const char *target_name = SituationTarget(
            sim, situation, target, sizeof(target));
        Append(output, capacity, "  %d. ", i + 1);
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY) {
            Append(output, capacity,
                   "Deliver %d food boxes to %s for %s.\n",
                   situation->quantity, target_name,
                   situation->sponsor_name);
        } else if (situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            Append(output, capacity,
                   "%s's foxfire supper: eight food boxes by the road no soldier admits exists.\n",
                   situation->sponsor_name);
        } else if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
            Append(output, capacity,
                   "%s's iron chain: find out why a sound bridge will not open between %s.\n",
                   situation->sponsor_name, target_name);
        } else if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
            if (!CcSimSituationCanAccept(sim, situation)) {
                char objective[128];
                MineObjective(situation, speaker, objective,
                              sizeof(objective));
                Append(output, capacity, "%s\n", objective);
            } else {
                Append(output, capacity,
                       "Find Cera in the west gallery.\n");
            }
        } else {
            Append(output, capacity,
                   "%s's unopened letter: carry %s before its news grows old.\n",
                   situation->sponsor_name, target_name);
        }
        if (CcSimSituationCanAccept(sim, situation)) {
            Append(output, capacity,
                   "     Reward: %" PRId64 " crowns. Due by day %d%s.\n",
                   situation->reward, situation->deadline_day,
                   accepted != NULL && accepted->id == situation->id ?
                       " [your promise]" : "");
        }
        shown += 1;
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            Append(output, capacity, "     %s will meet you there.\n",
                   situation->affected_name);
        } else if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
            Append(output, capacity,
                   "     Reopen the road with 2 tools or 18 crowns. Tools are faster and last longer.\n");
        } else if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
            if (CcSimSituationCanAccept(sim, situation)) {
                Append(output, capacity,
                       "     Bring tools to the west gallery.\n");
            } else {
                Append(output, capacity,
                       "     No reward has been offered yet.\n");
            }
        } else {
            Append(output, capacity,
                   "     Carry the unopened letter in the carriage. Its orders take effect when it reaches the named court.\n");
        }
    }
    if (shown == 0) {
        Append(output, capacity,
               "  No fresh paper waits here. That does not mean nobody needs help.\n");
    }
    Append(output, capacity,
           "Use 'talk NUMBER' to ask about an objective. Use 'tell NUMBER' to tell Mara, or 'keep NUMBER' to keep it between you and Jory.\n");
}

static bool TalkToSituation(CcMetagame *metagame, int32_t index,
                            char *output, size_t capacity)
{
    CcSim *sim = &metagame->sim;
    CcSituation *situation = &sim->situations[index];
    const CcCharacter *speaker = CcSimSituationConversationCharacter(
        sim, situation, sim->player.location_id);
    if (situation->status != CC_SITUATION_ACTIVE || speaker == NULL) {
        Append(output, capacity,
               "Nobody here can tell that part of the story.\n");
        return false;
    }
    bool sponsor_here = speaker->id == situation->sponsor_character_id;
    bool affected_here = speaker->id == situation->affected_character_id;
    CcSituationDiscoveryStage stage_before = situation->discovery_stage;
    char spoken_text[192];
    if (!CcStoryCharacterText(
            sim, situation, speaker, spoken_text, sizeof(spoken_text))) {
        (void)snprintf(spoken_text, sizeof(spoken_text),
                       "Tell me what happened, from the beginning.");
    }
    bool advances_mine_lead =
        situation->kind == CC_SITUATION_MONSTER_EXPEDITION &&
        stage_before != CC_DISCOVERY_DECISION &&
        stage_before != CC_DISCOVERY_OFFER;
    if (advances_mine_lead &&
        !CcCharacterRemembers(speaker, CC_CHARACTER_MEMORY_MET_PLAYER,
                              situation->id)) {
        CcCommand listen = {
            .kind = CC_COMMAND_CHARACTER_RESPONSE,
            .target_id = situation->id,
            .amount = CC_CHARACTER_RESPONSE_LISTEN
        };
        if (!ApplyCommand(metagame, &listen, output, capacity)) {
            return false;
        }
    }

    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        if (stage_before == CC_DISCOVERY_RUMOR) {
            Append(output, capacity,
                   "Jory: \"%s\"\n"
                   "New objective: Talk to Bren.\n",
                   spoken_text);
        } else if (stage_before == CC_DISCOVERY_WITNESS) {
            Append(output, capacity,
                   "Bren: \"%s\"\n"
                   "New objective: Talk to Jory.\n",
                   spoken_text);
        } else if (stage_before == CC_DISCOVERY_DECISION) {
            Append(output, capacity,
                   "Jory: \"%s\"\n"
                   "Choose: 'tell %d' to tell Mara, or 'keep %d' to keep it between you and Jory.\n",
                   spoken_text, index + 1, index + 1);
        } else if (stage_before == CC_DISCOVERY_AUTHORITY) {
            Append(output, capacity,
                   "Mara: \"%s\"\n"
                   "New job: Find Cera in the west gallery. Use 'accept %d' to take the job.\n",
                   spoken_text, index + 1);
        } else {
            Append(output, capacity, "%s says, \"%s\"\n",
                   speaker->name, spoken_text);
        }
    } else if (situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
               sponsor_here) {
        Append(output, capacity,
               "%s: \"%s\"\n",
               situation->sponsor_name, spoken_text);
    } else if (situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
               sponsor_here) {
        Append(output, capacity,
               "%s unfolds from behind a stack of flour barrels. His green scarf is bright; his smile is brighter.\n"
               "\"%s\"\n"
               "For one moment the smile slips. \"My sister works the lower mine. They boiled their seed grain.\"\n"
               "Then it returns. \"Also, better pay.\"\n",
               situation->sponsor_name, spoken_text);
    } else if (situation->kind == CC_SITUATION_ROUTE_REPAIR &&
               sponsor_here) {
        Append(output, capacity,
               "%s sets an iron bridge key on the table. \"%s\"\n"
               "Two crates of tools would let her crew call the closed gate a repair. Eighteen crowns would buy the guards' silence.\n"
               "She watches the hungry boy on the wall finish his soup. \"If I open the gate, I am responsible for every food box that crosses.\"\n",
               situation->sponsor_name, spoken_text);
    } else if (situation->kind == CC_SITUATION_RELIEF_DELIVERY) {
        Append(output, capacity,
               "%s looks from the carriage boxes to the empty oven tins.\n"
               "\"%s\"\n",
               situation->affected_name, spoken_text);
    } else {
        Append(output, capacity,
               "%s turns the unopened letter over in both hands.\n"
               "\"%s\"\n",
               affected_here ? situation->affected_name :
                   situation->sponsor_name, spoken_text);
    }
    return true;
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
    Append(output, capacity, "Roads waiting beyond the last house:\n");
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != sim->player.location_id &&
            route->to_id != sim->player.location_id) continue;
        CcId destination_id = route->from_id == sim->player.location_id ?
            route->to_id : route->from_id;
        const CcSettlement *destination = CcSimSettlement(sim, destination_id);
        const CcMap *map = CcSimMapForRoute(sim, route->id, sim->player.id);
        CcTravelPreview preview = {0};
        if (!CcSimTravelPreview(sim, destination_id, &preview,
                                NULL, 0U)) continue;
        const char *road_name = preview.destination_known &&
                                destination != NULL ?
            destination->name : "unmarked track";
        Append(output, capacity,
               "  %d. %s toward %s — %d days, %" PRId64 " crowns, %d fodder, horse team %d%%, %s",
               i + 1, StoryRoadName(sim, route), road_name, preview.travel_days,
               preview.provision_cost, preview.horse_feed_required,
               preview.horse_readiness,
               route->closed ? "restricted and tolled" : "open");
        if (map != NULL) {
            Append(output, capacity,
                   ", %s notes say %s (%d days old)%s\n",
                   AccuracyWord(map->accuracy), DangerWord(map->recorded_danger),
                   sim->current_day - map->surveyed_day,
                   route->smuggler_route ? ", unmarked road" : "");
        } else if (preview.sponsored_guide) {
            Append(output, capacity,
                   ", the sponsor's guide knows the turns\n");
        } else {
            Append(output, capacity,
                   ", no notes: slower travel and unknown risk\n");
        }
    }
    Append(output, capacity, "Turn onto a branch with 'travel NUMBER'.\n");
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

static void DescribeAnimals(const CcMetagame *metagame,
                            char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity, "Carriage horses — team readiness %d%%:\n",
           CcSimHorseTeamReadiness(sim));
    for (int32_t i = 0; i < CcSimHorseCount(sim); ++i) {
        const CcHorse *horse = CcSimHorseAt(sim, i);
        const CcSettlement *stable = CcSimSettlement(
            sim, horse->stable_settlement_id);
        Append(output, capacity,
               "  %d. %s [%s] — %s, %s, age %d; health %d, fatigue %d, hunger %d, training %d\n",
               i + 1, horse->name,
               i < CC_CARRIAGE_HORSE_COUNT ?
                   (i == 0 ? "team 1" : "team 2") :
                   (stable != NULL ? stable->name : "boarded"),
               CcHorseSexName(horse->sex), CcHorseLifeStageName(horse),
               horse->age_days / 365, horse->health, horse->fatigue,
               horse->hunger, horse->training);
        Append(output, capacity,
               "     traits: strength %d, temperament %d, hardiness %d",
               horse->strength, horse->temperament, horse->hardiness);
        if (horse->sire_id != 0U || horse->dam_id != 0U) {
            const CcHorse *sire = CcSimHorse(sim, horse->sire_id);
            const CcHorse *dam = CcSimHorse(sim, horse->dam_id);
            Append(output, capacity, "; by %s out of %s",
                   sire != NULL ? sire->name : "unknown",
                   dam != NULL ? dam->name : "unknown");
        }
        if (horse->pregnant_by_id != 0U) {
            const CcHorse *sire = CcSimHorse(sim, horse->pregnant_by_id);
            Append(output, capacity, "; foal by %s due in %d days",
                   sire != NULL ? sire->name : "unknown",
                   horse->pregnancy_days_remaining);
        }
        Append(output, capacity, "\n");
    }
    Append(output, capacity,
           "At a stable: 'stable breed MARE STALLION' or 'stable team SLOT HORSE'.\n");
    Append(output, capacity, "Cattle herds:\n");
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->cow_adults + place->cow_calves <= 0) continue;
        Append(output, capacity,
               "  %s: %d cows, %d calves, condition %d, hunger %d\n",
               place->name, place->cow_adults, place->cow_calves,
               place->cow_condition, place->cow_hunger);
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
    if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING) {
        const CcSettlement *target = CcSimSettlement(
            sim, sim->goblins.tribute_target_id);
        Append(output, capacity,
               "%s is mustering to raid %s and leaves tomorrow.\n",
               sim->goblins.name,
               target != NULL ? target->name : "a rich town");
    } else if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND) {
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

static void DescribeGoblins(const CcMetagame *metagame,
                            char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcGoblinCult *goblins = &sim->goblins;
    const CcSettlement *lair = CcSimSettlement(
        sim, goblins->lair_settlement_id);
    const char *future = goblins->devotion >= 60 ?
        goblins->cohesion >= 60 ? "a united dragon court" :
                                  "fanatical ash-splinters" :
        goblins->cohesion >= 60 ? "a free lair beyond the dragon" :
                                  "scattered hungry bands";
    Append(output, capacity,
           "%s lives beneath %s. Nara Soot-Tongue speaks for its Hoardkeepers, Ashkeepers, Tongues, and Foragers.\n",
           goblins->name, lair != NULL ? lair->name : "an unknown lair");
    Append(output, capacity,
           "%d members; covenant %d/100, cohesion %d/100. Its present course points toward %s.\n",
           goblins->members, goblins->devotion, goblins->cohesion, future);
    Append(output, capacity,
           "Lair stores: %d Food, %d Tools, %d Weapons, %" PRId64
           " crowns. %d expeditions have been intercepted.\n",
           goblins->lair_stock[CC_GOOD_FOOD],
           goblins->lair_stock[CC_GOOD_TOOLS],
           goblins->lair_stock[CC_GOOD_WEAPONS], goblins->lair_coins,
           goblins->expeditions_intercepted);
    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING ||
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND) {
        const CcSettlement *target = CcSimSettlement(
            sim, goblins->tribute_target_id);
        Append(output, capacity,
               "An expedition threatens %s. It is %s%s.\n",
               target != NULL ? target->name : "an unknown settlement",
               goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING ?
                   "still mustering" : "on the road",
               goblins->target_warned ? "; the target has been warned" : "");
    } else if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING) {
        Append(output, capacity,
               "An expedition is returning to the lair with physical loot.\n");
    } else if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_TO_DRAGON) {
        Append(output, capacity,
               "A tribute bearer is taking portable spoils to the dragon cave.\n");
    } else {
        Append(output, capacity, "No expedition is active.\n");
    }
    if (goblins->dragon_seed_phase != CC_GOBLIN_DRAGON_SEED_NONE) {
        Append(output, capacity,
               "The ash-vault project is %s, with about %d years left before it can reveal a dragon seed.\n",
               goblins->dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_RUMORED ?
                   "an open rumor" : "under public preparation",
               (goblins->dragon_seed_days_remaining + 364) / 365);
    }
    if (sim->player.location_id == goblins->lair_settlement_id) {
        Append(output, capacity,
               "Here you may use 'goblins trade food|tools|weapons COUNT'.\n");
    }
    if (sim->player.location_id == goblins->tribute_target_id &&
        (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING ||
         goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND)) {
        Append(output, capacity,
               "Here you may use 'goblins warn' or 'goblins intercept'.\n");
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
            Append(output, capacity,
                   "; fields %d%%, cattle %d + %d calves",
                   place->field_yield, place->cow_adults,
                   place->cow_calves);
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

static void DescribeKingdoms(const CcMetagame *metagame,
                             char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "THE KINGDOMS OF MEN\n"
           "A realm holds only what its food, money, orders, and people can reach.\n");
    for (int32_t kingdom_index = 0;
         kingdom_index < sim->kingdom_count; ++kingdom_index) {
        const CcKingdom *kingdom = &sim->kingdoms[kingdom_index];
        CcKingdomCalling calling = CcSimKingdomCalling(sim, kingdom->id);
        int32_t pressure = CcSimKingdomPressure(sim, kingdom->id);
        Append(output, capacity, "\n%s — %s\n",
               kingdom->name, CcKingdomCallingName(calling));
        Append(output, capacity, "  Holds: ");
        int32_t holdings = 0;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlement *place = &sim->settlements[i];
            if (place->kingdom_id != kingdom->id) continue;
            Append(output, capacity, "%s%s (%s)", holdings > 0 ? ", " : "",
                   place->name, CcSettlementFunctionName(place->function));
            holdings += 1;
        }
        if (holdings == 0) Append(output, capacity, "no settled seat");
        Append(output, capacity,
               ".\n  Power: %s.\n  Dependence: %s.\n"
               "  Contradiction: %s\n"
               "  Present %s: %d/100, %s. Treasury %" PRId64
               ", debt %" PRId64 ", legitimacy %d.\n",
               KingdomPower(calling), KingdomDependence(calling),
               KingdomContradiction(calling), KingdomPressureName(calling),
               pressure, KingdomPressureWord(pressure), kingdom->treasury,
               kingdom->iron_ledger_debt, kingdom->legitimacy);
        Append(output, capacity, "  Politics (support / material power):\n");
        for (int32_t i = 0; i < sim->faction_count; ++i) {
            const CcFaction *faction = &sim->factions[i];
            if (faction->kingdom_id != kingdom->id) continue;
            Append(output, capacity, "    %s: %d / %d\n",
                   CcFactionKindName(faction->kind), faction->support,
                   faction->power);
        }
        Append(output, capacity, "  Relations: ");
        int32_t relations = 0;
        for (int32_t other = 0; other < sim->kingdom_count; ++other) {
            if (other == kingdom_index) continue;
            Append(output, capacity, "%s%s — %s",
                   relations > 0 ? "; " : "",
                   sim->kingdoms[other].name,
                   CcDiplomaticStateName(
                       sim->diplomacy[kingdom_index][other]));
            relations += 1;
        }
        Append(output, capacity, ".\n");
    }
    Append(output, capacity,
           "\nThe Crownless Carriage serves no realm. Moving food, tools, weapons, maps, and unopened letters changes which of these claims can survive.\n");
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

static void DescribeUnderroad(const CcMetagame *metagame,
                              char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    if (!expedition->active) {
        const CcDungeon *dungeon = sim->dungeon_count > 0 ?
            &sim->dungeons[0] : NULL;
        if (dungeon == NULL) {
            Append(output, capacity, "No Underroad has been found.\n");
            return;
        }
        Append(output, capacity,
               "UNDERROAD — %s\n"
               "State: %s. Known rooms: ",
               dungeon->name, CcDungeonStateName(dungeon->state));
        int32_t known = 0;
        for (int32_t room = 0; room < dungeon->room_count; ++room) {
            if ((dungeon->rooms[room].state_flags &
                 CC_DUNGEON_ROOM_DISCOVERED) != 0U) known += 1;
        }
        Append(output, capacity, "%d of %d.\n", known, dungeon->room_count);
        if (sim->player.location_id == dungeon->settlement_id) {
            Append(output, capacity,
                   "The carriage is at the entrance. Carry Food, then use 'underroad enter'.\n");
        } else {
            Append(output, capacity,
                   "The entrance is at Silverwick. The carriage must travel there first.\n");
        }
        return;
    }
    const CcDungeon *dungeon = CcSimDungeon(sim, expedition->dungeon_id);
    const CcDungeonRoom *room = CcSimDungeonCurrentRoom(sim);
    if (dungeon == NULL || room == NULL) return;
    Append(output, capacity,
           "UNDERROAD — %s\n"
           "%s, depth %d — %s\n"
           "Turn %d. Light %d. Noise %d. Strain %d.\n",
           dungeon->name, room->name, room->depth,
           CcDungeonRoomKindName(room->kind),
           expedition->turns_elapsed, expedition->light_remaining,
           expedition->noise, expedition->strain);
    if ((room->flags & CC_DUNGEON_ROOM_HAZARD) != 0U) {
        Append(output, capacity,
               "The chamber itself looks dangerous.\n");
    }
    if ((room->flags & CC_DUNGEON_ROOM_STONEBACK) != 0U) {
        Append(output, capacity,
               "Tools have been arranged here in deliberate stone patterns.\n");
    }
    if ((room->flags & CC_DUNGEON_ROOM_DRAGON_SIGN) != 0U) {
        Append(output, capacity,
               "Warm air and old tribute marks lead deeper.\n");
    }
    if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
        Append(output, capacity,
               "Encounter: %s. Reaction %d, %s.\n"
               "Use 'underroad parley', 'underroad evade', 'underroad force', or 'underroad retreat'.\n",
               CcDungeonEncounterName(expedition->encounter_kind),
               expedition->encounter_reaction,
               CcDungeonReactionName(expedition->encounter_reaction));
        return;
    }
    int32_t exits = CcSimDungeonVisibleExitCount(sim);
    Append(output, capacity, "Passages:\n");
    for (int32_t i = 0; i < exits; ++i) {
        int32_t target = CcSimDungeonVisibleExitAt(sim, i);
        const CcDungeonRoom *next = target >= 0 ?
            &dungeon->rooms[target] : NULL;
        bool known = next != NULL &&
            (next->state_flags & CC_DUNGEON_ROOM_DISCOVERED) != 0U;
        Append(output, capacity, "  %d. %s\n", i + 1,
               known ? next->name : "unmapped passage");
    }
    Append(output, capacity,
           "Use 'underroad move NUMBER', 'underroad search', or 'underroad retreat'.\n");
    if (CcSimDungeonOpenableShortcut(sim) >= 0) {
        Append(output, capacity,
               "A blocked freight shortcut can be opened with 'underroad open' and 1 Tools.\n");
    }
}

static void DescribeHelp(char *output, size_t capacity)
{
    Append(output, capacity,
           "See the world:\n"
           "  look, people, talk NUMBER, rumors, charters, roads\n"
           "  causes, notes, cargo, animals, economy, treasures, inequality, kingdoms, war, dragon, goblins, archives, status, history [COUNT]\n"
           "  mark — compare this campaign against a no-action control of the same seed\n"
           "Make commitments:\n"
           "  tell NUMBER, keep NUMBER, accept NUMBER, refuse NUMBER, abandon\n"
           "Move goods and people:\n"
           "  buy food|iron|tools|weapons|gold|gems COUNT\n"
           "  sell food|iron|tools|weapons|gold|gems COUNT\n"
           "  buy-map NUMBER, sell-map NUMBER\n"
           "  buy-notes NUMBER, sell-notes NUMBER (aliases)\n"
           "  archive-map NUMBER, retrieve-map NUMBER (in Gloamgate)\n"
           "  buy-treasure NUMBER, sell-treasure NUMBER, travel NUMBER\n"
           "Act on the road and world:\n"
           "  road fight|bargain|supper|turn-back, repair NUMBER tools|cash\n"
           "  stable breed MARE STALLION, stable team SLOT HORSE\n"
           "  underroad enter|look|move NUMBER|search|open\n"
           "  underroad parley|evade|force|retreat\n"
           "  dungeon public|smuggler|close (after reaching the threshold), wait DAYS\n"
           "  dragon steal COUNT, dragon return COUNT (at the cave)\n"
           "  dragon steal-treasure NUMBER, dragon return-treasure\n"
           "  dragon intercept (when tribute approaches the cave)\n"
           "  goblins trade food|tools|weapons COUNT (at their lair)\n"
           "  goblins warn|intercept (at the threatened settlement)\n"
           "Keep the test:\n"
           "  save PATH, load PATH, debrief, quit\n");
}

static bool AdvanceRuntimeTicks(CcMetagame *metagame, int32_t ticks,
                                char *output, size_t capacity)
{
    if (metagame->journal == NULL) {
        CcSimAdvanceRuntimeTicks(&metagame->sim, ticks);
        return true;
    }
    char error[192];
    if (CcJournalAdvanceRuntimeTicks(metagame->journal, &metagame->sim,
                                     ticks, error, sizeof(error))) {
        return true;
    }
    Append(output, capacity, "%s\n", error);
    return false;
}

static bool AdvanceDays(CcMetagame *metagame, int32_t days,
                        char *output, size_t capacity)
{
    if (metagame->journal == NULL) {
        CcSimAdvanceDays(&metagame->sim, days);
        return true;
    }
    char error[192];
    if (CcJournalAdvanceDays(metagame->journal, &metagame->sim,
                             days, error, sizeof(error))) {
        return true;
    }
    Append(output, capacity, "%s\n", error);
    return false;
}

static bool FinishTravel(CcMetagame *metagame,
                         char *output, size_t capacity)
{
    CcSim *sim = &metagame->sim;
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        if (!AdvanceRuntimeTicks(metagame, CC_WORLD_TICKS_PER_SECOND,
                                 output, capacity)) return false;
    }
    if (sim->journey.active &&
        sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
        const CcEvent *event = CcSimRecentEvent(sim, 0);
        Append(output, capacity, "%s\n",
               event != NULL ? event->text : "The road is blocked.");
        Append(output, capacity,
               "Choose 'road bargain', 'road supper', 'road fight', or 'road turn-back'.\n");
    } else {
        const CcSettlement *place = CurrentPlace(metagame);
        if (IsNamedSettlement(sim, place, 1)) {
            Append(output, capacity,
                   "Gloamgate appears around its headless fountain on day %d. Bracken stops because the road does. Morrow stops because Bracken did.\n",
                   sim->current_day);
        } else if (IsNamedSettlement(sim, place, 3)) {
            Append(output, capacity,
                   "Silverwick climbs into view on day %d, all black steps and silver smoke. Its market clock is still waiting for breakfast.\n",
                   sim->current_day);
        } else {
            Append(output, capacity,
                   "The carriage reaches %s on day %d. Bracken looks pleased with himself. Morrow looks pleased with Bracken.\n",
                   place != NULL ? place->name : "an unknown place",
                   sim->current_day);
        }
    }
    return true;
}

static bool ApplyCommand(CcMetagame *metagame, const CcCommand *command,
                         char *output, size_t capacity)
{
    char error[192];
    bool applied = metagame->journal != NULL ?
        CcJournalApply(metagame->journal, &metagame->sim, command,
                       error, sizeof(error)) :
        CcSimApply(&metagame->sim, command, error, sizeof(error));
    if (!applied) {
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
    metagame->sim.player.location_id = metagame->sim.settlements[0].id;
    metagame->sim.carriage.location_id = metagame->sim.player.location_id;
    metagame->sim.player.coins = 75;
}

bool CcMetagameStartJournal(CcMetagame *metagame, const char *path,
                            char *error, size_t error_capacity)
{
    if (metagame == NULL || metagame->journal != NULL) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity,
                           "Metagame already has an action journal.");
        }
        return false;
    }
    metagame->journal = CcJournalStart(
        path, &metagame->sim, error, error_capacity);
    return metagame->journal != NULL;
}

bool CcMetagameResumeJournal(CcMetagame *metagame, const char *path,
                             char *error, size_t error_capacity)
{
    if (metagame == NULL || metagame->journal != NULL) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity,
                           "Metagame already has an action journal.");
        }
        return false;
    }
    CcJournal *journal = CcJournalResume(
        path, &metagame->sim, error, error_capacity);
    if (journal == NULL) return false;
    metagame->quit_requested = false;
    metagame->journal = journal;
    return true;
}

bool CcMetagameCloseJournal(CcMetagame *metagame,
                            char *error, size_t error_capacity)
{
    if (metagame == NULL) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity, "Metagame is missing.");
        }
        return false;
    }
    return CcJournalClose(&metagame->journal, &metagame->sim,
                          error, error_capacity);
}

void CcMetagameIntro(const CcMetagame *metagame,
                     char *output, size_t output_capacity)
{
    if (output == NULL || output_capacity == 0U) return;
    output[0] = '\0';
    Append(output, output_capacity,
           "CROWNLESS CARRIAGE — THE ROAD WITHOUT A CROWN\n\n"
           "You arrive in Thornford on foot. Mara Venn is waiting by the town board.\n\n");
    const CcSim *sim = &metagame->sim;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->kind != CC_SITUATION_RELIEF_DELIVERY ||
            CcSimSituationOfferSettlementId(sim, situation) !=
                sim->player.location_id) continue;
        const CcCharacter *speaker = CcSimSituationConversationCharacter(
            sim, situation, sim->player.location_id);
        char spoken[192];
        if (speaker != NULL && CcStoryCharacterText(
                sim, situation, speaker, spoken, sizeof(spoken))) {
            Append(output, output_capacity, "%s: \"%s\"\n\n",
                   speaker->name, spoken);
        }
        break;
    }
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
    char *third = strtok(NULL, " \t");
    if (command == NULL) return true;

    if (strcmp(command, "help") == 0) DescribeHelp(output, output_capacity);
    else if (strcmp(command, "look") == 0) {
        DescribeLook(metagame, output, output_capacity);
    } else if (strcmp(command, "causes") == 0) {
        DescribeCauses(metagame, output, output_capacity);
    } else if (strcmp(command, "people") == 0) {
        DescribePeople(metagame, output, output_capacity);
    } else if (strcmp(command, "talk") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.situation_count, &index)) {
            Append(output, output_capacity,
                   "Choose a number from 'charters'.\n");
            return false;
        }
        if (!TalkToSituation(metagame, index, output, output_capacity)) {
            return false;
        }
    } else if (strcmp(command, "rumors") == 0) {
        DescribeRumors(metagame, output, output_capacity);
    } else if (strcmp(command, "charters") == 0) {
        DescribeCharters(metagame, output, output_capacity);
    } else if (strcmp(command, "roads") == 0 ||
               strcmp(command, "routes") == 0) {
        DescribeRoutes(metagame, output, output_capacity);
    } else if (strcmp(command, "notes") == 0 ||
               strcmp(command, "maps") == 0) {
        DescribeMaps(metagame, output, output_capacity);
    } else if (strcmp(command, "cargo") == 0) {
        DescribeCargo(metagame, output, output_capacity);
    } else if (strcmp(command, "animals") == 0) {
        DescribeAnimals(metagame, output, output_capacity);
    } else if (strcmp(command, "stable") == 0) {
        int32_t first_index = 0;
        int32_t second_index = 0;
        CcCommand action = {0};
        if (first != NULL && strcmp(first, "breed") == 0 &&
            ParseIndex(second, CcSimHorseCount(&metagame->sim),
                       &first_index) &&
            ParseIndex(third, CcSimHorseCount(&metagame->sim),
                       &second_index)) {
            action.kind = CC_COMMAND_BREED_HORSES;
            action.target_id = CcSimHorseAt(
                &metagame->sim, first_index)->id;
            action.amount = second_index + 1;
        } else if (first != NULL && strcmp(first, "team") == 0 &&
                   ParseIndex(second, CC_CARRIAGE_HORSE_COUNT,
                              &first_index) &&
                   ParseIndex(third, CcSimHorseCount(&metagame->sim),
                              &second_index)) {
            action.kind = CC_COMMAND_ASSIGN_HORSE;
            action.target_id = CcSimHorseAt(
                &metagame->sim, second_index)->id;
            action.amount = first_index + 1;
        } else {
            Append(output, output_capacity,
                   "Use 'stable breed MARE STALLION' or 'stable team SLOT HORSE' with numbers from 'animals'.\n");
            return false;
        }
        if (!ApplyCommand(metagame, &action, output, output_capacity)) {
            return false;
        }
        DescribeAnimals(metagame, output, output_capacity);
    } else if (strcmp(command, "economy") == 0) {
        DescribeEconomy(metagame, output, output_capacity);
    } else if (strcmp(command, "treasures") == 0) {
        DescribeTreasures(metagame, output, output_capacity);
    } else if (strcmp(command, "inequality") == 0) {
        DescribeInequality(metagame, output, output_capacity);
    } else if (strcmp(command, "kingdoms") == 0 ||
               strcmp(command, "realms") == 0) {
        DescribeKingdoms(metagame, output, output_capacity);
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
        } else if (strcmp(first, "intercept") == 0 && second == NULL) {
            action.kind = CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE;
        } else {
            Append(output, output_capacity,
                   "Use 'dragon steal COUNT', 'dragon return COUNT', 'dragon steal-treasure NUMBER', 'dragon return-treasure', or 'dragon intercept'.\n");
            return false;
        }
        action.amount = amount;
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        DescribeDragon(metagame, output, output_capacity);
    } else if (strcmp(command, "goblins") == 0 && first == NULL) {
        DescribeGoblins(metagame, output, output_capacity);
    } else if (strcmp(command, "goblins") == 0) {
        CcCommand action = {0};
        if (strcmp(first, "trade") == 0 &&
            ParseGood(second, &action.good) &&
            ParseAmount(third, &action.amount)) {
            action.kind = CC_COMMAND_GOBLIN_TRADE;
        } else if (strcmp(first, "warn") == 0 && second == NULL) {
            action.kind = CC_COMMAND_GOBLIN_WARN;
        } else if (strcmp(first, "intercept") == 0 && second == NULL) {
            action.kind = CC_COMMAND_GOBLIN_INTERCEPT;
        } else {
            Append(output, output_capacity,
                   "Use 'goblins trade food|tools|weapons COUNT', 'goblins warn', or 'goblins intercept'.\n");
            return false;
        }
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        DescribeGoblins(metagame, output, output_capacity);
    } else if (strcmp(command, "status") == 0) {
        DescribeStatus(metagame, output, output_capacity);
    } else if (strcmp(command, "archives") == 0) {
        const CcArchives *a = &metagame->sim.archives;
        Append(output, output_capacity,
               "The archive holds %d pieces of lore, kept by %d scribe%s.\n"
               "Lost to decay: %d. Last record: day %d.\n"
               "The monastery reserve funds the scriptorium: %d crowns.\n",
               a->lore_stored, a->scribes, a->scribes == 1 ? "" : "s",
               a->lore_lost_total, a->last_recorded_day,
               (int)metagame->sim.iron_ledger_reserve);
        if (a->scribes == 0 && a->lore_stored > 0) {
            Append(output, output_capacity,
                   "No scribe remains. The archive is unwatched.\n");
        }
    } else if (strcmp(command, "mark") == 0) {
        uint64_t control_hash = 0U;
        if (!CcMetagameAgentCounterfactual(metagame, output,
                                           output_capacity, &control_hash)) {
            return false;
        }
    } else if (strcmp(command, "history") == 0) {
        int32_t count = 8;
        if (first != NULL && (!ParseDays(first, &count) || count > 20)) {
            Append(output, output_capacity, "History count must be 1 to 20.\n");
            return false;
        }
        DescribeHistory(metagame, count, output, output_capacity);
    } else if (strcmp(command, "debrief") == 0) {
        DescribeDebrief(metagame, output, output_capacity);
    } else if (strcmp(command, "tell") == 0 ||
               strcmp(command, "keep") == 0 ||
               strcmp(command, "report") == 0 ||
               strcmp(command, "confide") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.situation_count, &index)) {
            Append(output, output_capacity, "Choose a lead number.\n");
            return false;
        }
        CcCommand action = {
            .kind = CC_COMMAND_CHARACTER_RESPONSE,
            .target_id = metagame->sim.situations[index].id,
            .amount = strcmp(command, "tell") == 0 ||
                    strcmp(command, "report") == 0 ?
                CC_CHARACTER_RESPONSE_REPORT_EVIDENCE :
                CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) {
            return false;
        }
        const CcSituation *situation = &metagame->sim.situations[index];
        CcId next_location = CcSimSituationOfferSettlementId(
            &metagame->sim, situation);
        const CcSettlement *next_place = CcSimSettlement(
            &metagame->sim, next_location);
        const CcCharacter *next = CcSimSituationConversationCharacter(
            &metagame->sim, situation, next_location);
        if (CcSimSituationCanAccept(&metagame->sim, situation)) {
            Append(output, output_capacity,
                   "Jory asks you to help find Cera. You can accept the job now.\n");
        } else {
            Append(output, output_capacity,
                   "New objective: Talk to %s. %s is in %s.\n",
                   next != NULL ? next->name : situation->sponsor_name,
                   next != NULL ? next->name : situation->sponsor_name,
                   next_place != NULL ? next_place->name : "the next town");
        }
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
        const CcSituation *situation = &metagame->sim.situations[index];
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY) {
            Append(output, output_capacity,
                   "You accept %s's job. Mara loads all %d food boxes into the carriage at no charge.\n",
                   situation->sponsor_name, situation->quantity);
        } else {
            Append(output, output_capacity,
                   "You accept %s's job. You can only carry one job at a time.\n",
                   situation->sponsor_name);
        }
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
               "%s hears the refusal without interrupting. Outside, the road goes on waiting.\n",
               metagame->sim.situations[index].sponsor_name);
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
        const CcSituation *delivery = CcSimAcceptedSituation(
            &metagame->sim);
        bool story_delivery = amount < 0 && good == CC_GOOD_FOOD &&
            delivery != NULL &&
            delivery->target_id == metagame->sim.player.location_id &&
            (delivery->kind == CC_SITUATION_RELIEF_DELIVERY ||
             delivery->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
        CcCommand action = {
            .kind = CC_COMMAND_TRADE,
            .good = good,
            .amount = amount
        };
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        if (story_delivery &&
            delivery->status == CC_SITUATION_RESOLVED &&
            delivery->kind == CC_SITUATION_RELIEF_DELIVERY) {
            Append(output, output_capacity,
                   "The carriage doors open beneath Silverwick's stopped clock. A merchant's clerk reaches for Mara's receipt; children at the town ovens reach for the smell of flour. Jory watches both.\n"
                   "All eight food boxes leave the carriage. No golden light declares the choice good. The first oven simply grows warm.\n");
        } else if (story_delivery &&
                   delivery->status == CC_SITUATION_RESOLVED) {
            Append(output, output_capacity,
                   "A woman in a fox mask rolls her cart from the side alley. Eight food boxes vanish beneath patched blankets and reappear, one by one, beside family ovens.\n"
                   "The merchant's clerk keeps his receipt. The children get bread. Far away, somebody begins painting a new toll sign.\n");
        } else {
            Append(output, output_capacity, "%s %d %s.\n",
                   amount > 0 ? "Loaded" : "Unloaded",
                   amount > 0 ? amount : -amount, CcGoodName(good));
        }
    } else if (strcmp(command, "buy-map") == 0 ||
               strcmp(command, "sell-map") == 0 ||
               strcmp(command, "buy-notes") == 0 ||
               strcmp(command, "sell-notes") == 0 ||
               strcmp(command, "archive-map") == 0 ||
               strcmp(command, "retrieve-map") == 0) {
        int32_t index;
        if (!ParseIndex(first, metagame->sim.map_count, &index)) {
            Append(output, output_capacity, "Choose a map number.\n");
            return false;
        }
        bool buying = strcmp(command, "buy-map") == 0 ||
                      strcmp(command, "buy-notes") == 0;
        bool selling = strcmp(command, "sell-map") == 0 ||
                       strcmp(command, "sell-notes") == 0;
        CcCommandKind kind = buying ? CC_COMMAND_BUY_MAP :
            selling ? CC_COMMAND_SELL_MAP :
            strcmp(command, "archive-map") == 0 ?
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
        if (!FinishTravel(metagame, output, output_capacity)) return false;
    } else if (strcmp(command, "road") == 0) {
        int32_t condition_before = metagame->sim.carriage.condition;
        CcMoney coins_before = metagame->sim.player.coins;
        int32_t food_before = metagame->sim.player.cargo[CC_GOOD_FOOD];
        const CcRoute *blocked_route = CcSimRoute(
            &metagame->sim, metagame->sim.journey.route_id);
        bool king_chain = blocked_route != NULL && blocked_route->closed &&
            !blocked_route->smuggler_route;
        bool night_road = blocked_route != NULL &&
            blocked_route->smuggler_route;
        CcCommand action = {0};
        if (first != NULL && strcmp(first, "fight") == 0) {
            action.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT;
        } else if (first != NULL && strcmp(first, "bargain") == 0) {
            action.kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE;
        } else if (first != NULL && strcmp(first, "supper") == 0) {
            action.kind = CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS;
        } else if (first != NULL && strcmp(first, "turn-back") == 0) {
            action.kind = CC_COMMAND_WITHDRAW_ENCOUNTER;
        } else {
            Append(output, output_capacity,
                   "Choose 'road bargain', 'road supper', 'road fight', or 'road turn-back'.\n");
            return false;
        }
        if (!ApplyCommand(metagame, &action, output, output_capacity)) return false;
        if (king_chain && strcmp(first, "bargain") == 0) {
            Append(output, output_capacity,
                   "Ilyra cuts the chain herself. She says her report will blame the bridge machinery.\n");
        } else if (night_road && strcmp(first, "supper") == 0) {
            Append(output, output_capacity,
                   "The camp pot began with one onion and a great deal of hope. When the bowls are empty, the fox masks move aside.\n");
        } else if (night_road && strcmp(first, "bargain") == 0) {
            Append(output, output_capacity,
                   "Coins pass beneath the fox lantern. The road opens, and its collectors count what the road is teaching them.\n");
        } else if (strcmp(first, "turn-back") == 0) {
            Append(output, output_capacity,
                   "You turn the team around before anyone draws blood. A road refused is still a choice.\n");
        }
        if (metagame->sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            for (int32_t i = 0; i < 8; ++i) {
                const CcEvent *event = CcSimRecentEvent(&metagame->sim, i);
                if (event == NULL || event->kind != CC_EVENT_ENCOUNTER_LOOT) break;
                Append(output, output_capacity, "%s\n", event->text);
            }
        }
        if (food_before != metagame->sim.player.cargo[CC_GOOD_FOOD]) {
            Append(output, output_capacity,
                   "The road choice shares %d Food.\n",
                   food_before - metagame->sim.player.cargo[CC_GOOD_FOOD]);
        } else {
            Append(output, output_capacity,
                   "The road choice costs %" PRId64
                   " crowns and %d carriage condition.\n",
                   coins_before - metagame->sim.player.coins,
                   condition_before - metagame->sim.carriage.condition);
        }
        if (action.kind != CC_COMMAND_WITHDRAW_ENCOUNTER) {
            if (!FinishTravel(metagame, output, output_capacity)) return false;
        }
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
    } else if (strcmp(command, "underroad") == 0) {
        CcCommand action = {0};
        if (first == NULL || strcmp(first, "look") == 0) {
            DescribeUnderroad(metagame, output, output_capacity);
            return true;
        } else if (strcmp(first, "enter") == 0) {
            if (metagame->sim.dungeon_count < 1) return false;
            action.kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION;
            action.target_id = metagame->sim.dungeons[0].id;
        } else if (strcmp(first, "move") == 0) {
            int32_t exit_index = 0;
            int32_t exit_count = CcSimDungeonVisibleExitCount(
                &metagame->sim);
            if (!ParseIndex(second, exit_count, &exit_index)) {
                Append(output, output_capacity,
                       "Choose a passage number from 'underroad look'.\n");
                return false;
            }
            action.kind = CC_COMMAND_MOVE_DUNGEON;
            action.amount = CcSimDungeonVisibleExitAt(
                &metagame->sim, exit_index);
        } else if (strcmp(first, "search") == 0) {
            action.kind = CC_COMMAND_SEARCH_DUNGEON;
        } else if (strcmp(first, "open") == 0) {
            int32_t shortcut = CcSimDungeonOpenableShortcut(
                &metagame->sim);
            if (shortcut < 0) {
                Append(output, output_capacity,
                       "No discovered shortcut can be opened here.\n");
                return false;
            }
            action.kind = CC_COMMAND_OPEN_DUNGEON_SHORTCUT;
            action.amount = shortcut;
        } else if (strcmp(first, "parley") == 0) {
            action.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            action.amount = CC_DUNGEON_APPROACH_PARLEY;
        } else if (strcmp(first, "evade") == 0) {
            action.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            action.amount = CC_DUNGEON_APPROACH_EVADE;
        } else if (strcmp(first, "force") == 0) {
            action.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            action.amount = CC_DUNGEON_APPROACH_FORCE;
        } else if (strcmp(first, "retreat") == 0) {
            action.kind = CC_COMMAND_RETREAT_DUNGEON;
        } else {
            Append(output, output_capacity,
                   "Use 'underroad enter', 'underroad look', 'underroad move NUMBER', 'underroad search', 'underroad open', 'underroad parley', 'underroad evade', 'underroad force', or 'underroad retreat'.\n");
            return false;
        }
        if (!ApplyCommand(metagame, &action, output, output_capacity)) {
            return false;
        }
        DescribeUnderroad(metagame, output, output_capacity);
    } else if (strcmp(command, "dungeon") == 0) {
        CcDungeonState state;
        if (first != NULL && strcmp(first, "public") == 0) {
            state = CC_DUNGEON_PUBLIC_ROUTE;
        } else if (first != NULL && strcmp(first, "smuggler") == 0) {
            state = CC_DUNGEON_SMUGGLER_ROUTE;
        } else if (first != NULL &&
                   (strcmp(first, "close") == 0 ||
                    strcmp(first, "seal") == 0)) {
            state = CC_DUNGEON_RESEALED;
        } else {
            Append(output, output_capacity,
                   "Choose 'dungeon public', 'dungeon smuggler', or 'dungeon close'.\n");
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
        if (metagame->sim.journey.active ||
            metagame->sim.dungeon_expedition.active) {
            Append(output, output_capacity,
                   metagame->sim.dungeon_expedition.active ?
                       "Return from the Underroad before waiting.\n" :
                       "Resolve the road before waiting.\n");
            return false;
        }
        if (!AdvanceDays(metagame, days, output, output_capacity)) return false;
        Append(output, output_capacity,
               "%d days pass. Bread rises, letters travel, and people make plans in rooms where your chair is empty.\n",
               days);
        DescribeLook(metagame, output, output_capacity);
    } else if (strcmp(command, "save") == 0) {
        char error[192];
        if (metagame->journal != NULL) {
            Append(output, output_capacity,
                   "This campaign already has a live action journal.\n");
            return false;
        }
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
        if (metagame->journal != NULL) {
            Append(output, output_capacity,
                   "Close the live action journal before loading another campaign.\n");
            return false;
        }
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

static void DescribeAgentPossessions(const CcMetagame *metagame,
                                     char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity,
           "Your carriage condition is %d/100. Horse-team readiness is %d%%.\n",
           sim->carriage.condition, CcSimHorseTeamReadiness(sim));
    DescribeCargo(metagame, output, capacity);
    DescribeMaps(metagame, output, capacity);
    Append(output, capacity, "Named treasures you can reach here:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        bool carried = treasure->owner_id == sim->player.id;
        bool for_sale = treasure->owner_id == sim->player.location_id;
        if (treasure->destroyed || (!carried && !for_sale)) continue;
        Append(output, capacity,
               "  %d. %s — value %d crowns [%s]\n",
               i + 1, treasure->name, treasure->appraised_value,
               carried ? "carried" : "for sale here");
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  None.\n");
}

static void DescribeAgentConsequences(const CcMetagame *metagame,
                                      char *output, size_t capacity)
{
    const CcSim *sim = &metagame->sim;
    Append(output, capacity, "Consequences witnessed or heard here:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->event_count && shown < 6; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL || event->location_id != sim->player.location_id) {
            continue;
        }
        Append(output, capacity, "  day %d: %s\n", event->day, event->text);
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  Nothing has reached you yet.\n");
}

static void DescribeAgentActions(char *output, size_t capacity)
{
    Append(output, capacity,
           "Send exactly one command on the next line. Available command families:\n"
           "  look, people, talk NUMBER, rumors, charters, roads, causes, notes, cargo, status\n"
           "  tell NUMBER, keep NUMBER, accept NUMBER, refuse NUMBER, abandon\n"
           "  buy GOOD COUNT, sell GOOD COUNT, buy-map NUMBER, sell-map NUMBER\n"
           "  archive-map NUMBER, retrieve-map NUMBER\n"
           "  buy-treasure NUMBER, sell-treasure NUMBER, travel NUMBER\n"
           "  road fight|bargain|supper|turn-back, repair NUMBER tools|cash\n"
           "  underroad enter|look|move NUMBER|search|open|parley|evade|force|retreat\n"
           "  dungeon public|smuggler|close, wait DAYS\n"
           "  dragon steal COUNT, dragon return COUNT, dragon steal-treasure NUMBER\n"
           "  dragon return-treasure, dragon intercept\n"
           "  goblins trade GOOD COUNT, goblins warn, goblins intercept\n"
           "  debrief, quit\n"
           "Global ledgers, hidden faction state, and save control are outside your reach.\n");
}

void CcMetagameAgentObserve(const CcMetagame *metagame,
                            char *output, size_t output_capacity)
{
    if (output == NULL || output_capacity == 0U) return;
    output[0] = '\0';
    if (metagame == NULL) {
        Append(output, output_capacity, "No courier is present.\n");
        return;
    }
    Append(output, output_capacity,
           "You are the Crownless Company courier. You know what you carry, what you have witnessed, and what people at your present place can tell you. You do not know hidden world state.\n");
    if (metagame->sim.dungeon_expedition.active) {
        DescribeUnderroad(metagame, output, output_capacity);
    } else {
        DescribeLook(metagame, output, output_capacity);
        if (metagame->sim.journey.active &&
            metagame->sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
            const CcEvent *event = CcSimRecentEvent(&metagame->sim, 0);
            Append(output, output_capacity, "The road has stopped you: %s\n",
                   event != NULL ? event->text : "The way is blocked.");
        }
        DescribePeople(metagame, output, output_capacity);
        DescribeRumors(metagame, output, output_capacity);
        DescribeCharters(metagame, output, output_capacity);
        DescribeRoutes(metagame, output, output_capacity);
        DescribeCauses(metagame, output, output_capacity);
    }
    DescribeAgentPossessions(metagame, output, output_capacity);
    DescribeAgentConsequences(metagame, output, output_capacity);
    DescribeAgentActions(output, output_capacity);
}

static bool AgentSituationVisible(const CcMetagame *metagame,
                                  const char *number)
{
    int32_t index = 0;
    if (!ParseIndex(number, metagame->sim.situation_count, &index)) {
        return false;
    }
    const CcSituation *situation = &metagame->sim.situations[index];
    return situation->status == CC_SITUATION_ACTIVE &&
        (CcSimSituationOfferSettlementId(&metagame->sim, situation) ==
             metagame->sim.player.location_id ||
         CcSimSituationConversationCharacter(
             &metagame->sim, situation,
             metagame->sim.player.location_id) != NULL);
}

static bool AgentRouteVisible(const CcMetagame *metagame,
                              const char *number)
{
    int32_t index = 0;
    if (!ParseIndex(number, metagame->sim.route_count, &index)) return false;
    const CcRoute *route = &metagame->sim.routes[index];
    return route->from_id == metagame->sim.player.location_id ||
        route->to_id == metagame->sim.player.location_id;
}

static bool AgentMapVisible(const CcMetagame *metagame,
                            const char *number)
{
    int32_t index = 0;
    if (!ParseIndex(number, metagame->sim.map_count, &index)) return false;
    const CcMap *map = &metagame->sim.maps[index];
    return map->owner_id == metagame->sim.player.id ||
        map->owner_id == metagame->sim.player.location_id;
}

static bool AgentTreasureVisible(const CcMetagame *metagame,
                                 const char *number)
{
    int32_t index = 0;
    if (!ParseIndex(number, metagame->sim.treasure_count, &index)) {
        return false;
    }
    const CcTreasure *treasure = &metagame->sim.treasures[index];
    return !treasure->destroyed &&
        (treasure->owner_id == metagame->sim.player.id ||
         treasure->owner_id == metagame->sim.player.location_id);
}

static bool AgentCommandAllowed(const CcMetagame *metagame,
                                const char *line)
{
    if (metagame == NULL || line == NULL || strlen(line) >= 256U) {
        return false;
    }
    char copy[256];
    (void)snprintf(copy, sizeof(copy), "%s", line);
    char *command = strtok(copy, " \t\r\n");
    char *first = strtok(NULL, " \t\r\n");
    char *second = strtok(NULL, " \t\r\n");
    if (command == NULL) return false;
    if (strcmp(command, "help") == 0 || strcmp(command, "look") == 0 ||
        strcmp(command, "people") == 0 ||
        strcmp(command, "rumors") == 0 ||
        strcmp(command, "charters") == 0 ||
        strcmp(command, "roads") == 0 || strcmp(command, "routes") == 0 ||
        strcmp(command, "causes") == 0 || strcmp(command, "notes") == 0 ||
        strcmp(command, "maps") == 0 || strcmp(command, "cargo") == 0 ||
        strcmp(command, "status") == 0 ||
        strcmp(command, "abandon") == 0 || strcmp(command, "buy") == 0 ||
        strcmp(command, "sell") == 0 || strcmp(command, "wait") == 0 ||
        strcmp(command, "debrief") == 0 || strcmp(command, "quit") == 0) {
        return true;
    }
    if (strcmp(command, "talk") == 0 || strcmp(command, "tell") == 0 ||
        strcmp(command, "keep") == 0 || strcmp(command, "report") == 0 ||
        strcmp(command, "confide") == 0 ||
        strcmp(command, "accept") == 0 ||
        strcmp(command, "refuse") == 0) {
        return AgentSituationVisible(metagame, first);
    }
    if (strcmp(command, "travel") == 0 ||
        strcmp(command, "repair") == 0) {
        return AgentRouteVisible(metagame, first);
    }
    if (strcmp(command, "buy-map") == 0 ||
        strcmp(command, "sell-map") == 0 ||
        strcmp(command, "buy-notes") == 0 ||
        strcmp(command, "sell-notes") == 0 ||
        strcmp(command, "archive-map") == 0 ||
        strcmp(command, "retrieve-map") == 0) {
        return AgentMapVisible(metagame, first);
    }
    if (strcmp(command, "buy-treasure") == 0 ||
        strcmp(command, "sell-treasure") == 0) {
        return AgentTreasureVisible(metagame, first);
    }
    if (strcmp(command, "road") == 0) {
        return metagame->sim.journey.active &&
            metagame->sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED;
    }
    if (strcmp(command, "underroad") == 0 ||
        strcmp(command, "dungeon") == 0) {
        return metagame->sim.dungeon_expedition.active ||
            (metagame->sim.dungeon_count > 0 &&
             metagame->sim.player.location_id ==
                 metagame->sim.dungeons[0].settlement_id);
    }
    if (strcmp(command, "dragon") == 0 && first != NULL) {
        return metagame->sim.player.location_id ==
            metagame->sim.dragon.lair_settlement_id;
    }
    if (strcmp(command, "goblins") == 0 && first != NULL) {
        if (strcmp(first, "trade") == 0) {
            return metagame->sim.player.location_id ==
                metagame->sim.goblins.lair_settlement_id;
        }
        bool intervention = strcmp(first, "warn") == 0 ||
            strcmp(first, "intercept") == 0;
        bool active = metagame->sim.goblins.tribute_phase ==
                CC_GOBLIN_TRIBUTE_PREPARING ||
            metagame->sim.goblins.tribute_phase ==
                CC_GOBLIN_TRIBUTE_OUTBOUND;
        return intervention && second == NULL && active &&
            metagame->sim.player.location_id ==
                metagame->sim.goblins.tribute_target_id;
    }
    return false;
}

bool CcMetagameAgentExecute(CcMetagame *metagame, const char *line,
                            char *output, size_t output_capacity)
{
    if (output == NULL || output_capacity == 0U) return false;
    output[0] = '\0';
    if (!AgentCommandAllowed(metagame, line)) {
        Append(output, output_capacity,
               "The courier boundary rejects that command. Act from local observations; hidden world reports and save control are not available.\n");
        return false;
    }
    char copy[256];
    (void)snprintf(copy, sizeof(copy), "%s", line);
    char *command = strtok(copy, " \t\r\n");
    if (command != NULL && strcmp(command, "help") == 0) {
        DescribeAgentActions(output, output_capacity);
        return true;
    }
    return CcMetagameExecute(metagame, line, output, output_capacity);
}

static bool AgentEventsMatch(const CcEvent *left, const CcEvent *right)
{
    return left != NULL && right != NULL && left->kind == right->kind &&
        left->day == right->day && left->location_id == right->location_id &&
        strcmp(left->text, right->text) == 0;
}

static bool AgentEventAppearsIn(const CcEvent *event, const CcSim *sim)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        if (AgentEventsMatch(event, CcSimRecentEvent(sim, i))) return true;
    }
    return false;
}

static void DescribeCounterfactualEvents(const CcSim *actual,
                                         const CcSim *control,
                                         char *output, size_t capacity)
{
    Append(output, capacity,
           "Consequences present only in the courier branch:\n");
    int32_t shown = 0;
    for (int32_t i = 0; i < actual->event_count && shown < 8; ++i) {
        const CcEvent *event = CcSimRecentEvent(actual, i);
        if (event == NULL || AgentEventAppearsIn(event, control)) continue;
        Append(output, capacity, "  day %d: %s\n", event->day, event->text);
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  None recorded.\n");

    Append(output, capacity,
           "Consequences present only when the company took no action:\n");
    shown = 0;
    for (int32_t i = 0; i < control->event_count && shown < 8; ++i) {
        const CcEvent *event = CcSimRecentEvent(control, i);
        if (event == NULL || AgentEventAppearsIn(event, actual)) continue;
        Append(output, capacity, "  day %d: %s\n", event->day, event->text);
        shown += 1;
    }
    if (shown == 0) Append(output, capacity, "  None recorded.\n");
}

bool CcMetagameAgentCounterfactual(const CcMetagame *metagame,
                                   char *output, size_t output_capacity,
                                   uint64_t *control_hash)
{
    if (output == NULL || output_capacity == 0U) return false;
    output[0] = '\0';
    if (metagame == NULL || control_hash == NULL ||
        metagame->sim.current_day < 1) {
        Append(output, output_capacity,
               "The no-action control could not be constructed.\n");
        return false;
    }
    CcMetagame *control = calloc(1U, sizeof(*control));
    if (control == NULL) {
        Append(output, output_capacity,
               "The no-action control could not be allocated.\n");
        return false;
    }
    CcMetagameInit(control, metagame->sim.world_seed);
    int32_t elapsed_days = metagame->sim.current_day - control->sim.current_day;
    if (elapsed_days > 0) CcSimAdvanceDays(&control->sim, elapsed_days);
    *control_hash = CcSimHash(&control->sim);

    Append(output, output_capacity,
           "NO-ACTION CONTROL — seed %" PRIu32 ", day %d\n"
           "The control world used the same seed and elapsed days while the company took no actions.\n"
           "Actual state %016" PRIx64 "; control state %016" PRIx64 ".\n",
           metagame->sim.world_seed, metagame->sim.current_day,
           CcSimHash(&metagame->sim), *control_hash);
    Append(output, output_capacity,
           "Settlement differences (actual / control):\n");
    int32_t changed = 0;
    int32_t settlement_count = metagame->sim.settlement_count <
            control->sim.settlement_count ?
        metagame->sim.settlement_count : control->sim.settlement_count;
    for (int32_t i = 0; i < settlement_count; ++i) {
        const CcSettlement *actual = &metagame->sim.settlements[i];
        const CcSettlement *idle = &control->sim.settlements[i];
        if (actual->stock[CC_GOOD_FOOD] == idle->stock[CC_GOOD_FOOD] &&
            actual->hunger == idle->hunger &&
            actual->security == idle->security) continue;
        Append(output, output_capacity,
               "  %s: Food %d/%d, hunger %d/%d, security %d/%d\n",
               actual->name, actual->stock[CC_GOOD_FOOD],
               idle->stock[CC_GOOD_FOOD], actual->hunger, idle->hunger,
               actual->security, idle->security);
        changed += 1;
    }
    if (changed == 0) Append(output, output_capacity, "  None.\n");

    int32_t actual_resolved = 0;
    int32_t actual_failed = 0;
    int32_t control_resolved = 0;
    int32_t control_failed = 0;
    for (int32_t i = 0; i < metagame->sim.situation_count; ++i) {
        if (metagame->sim.situations[i].status == CC_SITUATION_RESOLVED) {
            actual_resolved += 1;
        }
        if (metagame->sim.situations[i].status == CC_SITUATION_FAILED) {
            actual_failed += 1;
        }
    }
    for (int32_t i = 0; i < control->sim.situation_count; ++i) {
        if (control->sim.situations[i].status == CC_SITUATION_RESOLVED) {
            control_resolved += 1;
        }
        if (control->sim.situations[i].status == CC_SITUATION_FAILED) {
            control_failed += 1;
        }
    }
    Append(output, output_capacity,
           "Commitments resolved %d/%d; failed %d/%d (actual / control).\n"
           "Goblin cohesion %d/%d and covenant %d/%d. Dragon shadow %d/%d.\n",
           actual_resolved, control_resolved, actual_failed, control_failed,
           metagame->sim.goblins.cohesion, control->sim.goblins.cohesion,
           metagame->sim.goblins.devotion, control->sim.goblins.devotion,
           metagame->sim.dragon.regional_influence,
           control->sim.dragon.regional_influence);
    DescribeCounterfactualEvents(&metagame->sim, &control->sim,
                                 output, output_capacity);
    Append(output, output_capacity,
           "These are branch differences, not claims that one action had only one cause; later events can inherit earlier divergence.\n");
    free(control);
    return true;
}
