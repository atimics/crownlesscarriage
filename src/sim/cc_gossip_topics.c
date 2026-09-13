#include "sim/cc_gossip_topics.h"

#include <string.h>

static const char *const TopicNames[CC_GOSSIP_TOPIC_COUNT] = {
    "all",
    "dragon",
    "goblin",
    "war",
    "throne",
    "wheat",
    "herds",
    "ponies",
    "road",
    "bandit",
    "treasure",
};

bool CcGossipTopicMatches(CcGossipTopic topic, CcEventKind kind)
{
    switch (topic) {
        case CC_GOSSIP_TOPIC_DRAGON: /* dragon: the ember and its shadow */
            switch (kind) {
                case CC_EVENT_DRAGON_HOARD_STOLEN:
                case CC_EVENT_DRAGON_OMEN:
                case CC_EVENT_DRAGON_TREASURE_RETURNED:
                case CC_EVENT_DRAGON_RETALIATION:
                case CC_EVENT_DRAGON_MUSTERED:
                case CC_EVENT_DRAGON_BATTLE:
                case CC_EVENT_DRAGON_SLAIN:
                case CC_EVENT_DRAGON_HOARD_RECOVERED:
                case CC_EVENT_DRAGON_HUNT:
                case CC_EVENT_DRAGON_CROWNED:
                case CC_EVENT_DRAGON_UNCROWNED:
                case CC_EVENT_DRAGON_BROOD:
                case CC_EVENT_DRAGON_WHELP_DISPERSED:
                case CC_EVENT_DRAGON_AFTERSHOCK:
                case CC_EVENT_DRAGON_SUCCESSOR:
                case CC_EVENT_DRAGON_PATRON_NAMED:
                case CC_EVENT_DRAGON_TERRITORY_LOST:
                case CC_EVENT_GOBLIN_CULT_RALLIED:
                case CC_EVENT_GOBLIN_DRAGON_SEED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_GOBLIN: /* goblin: the Cinder Tithe and the underroad */
            switch (kind) {
                case CC_EVENT_GOBLIN_TRIBUTE_DEPARTED:
                case CC_EVENT_GOBLIN_TRIBUTE_TAKEN:
                case CC_EVENT_GOBLIN_TRIBUTE_DELIVERED:
                case CC_EVENT_GOBLIN_RAID_DEPARTED:
                case CC_EVENT_GOBLIN_RAIDED:
                case CC_EVENT_GOBLIN_RAID_RETURNED:
                case CC_EVENT_GOBLIN_HOARD_DEFENDED:
                case CC_EVENT_GOBLIN_RAID_PREPARED:
                case CC_EVENT_GOBLIN_TARGET_WARNED:
                case CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED:
                case CC_EVENT_GOBLIN_TRADE:
                case CC_EVENT_GOBLIN_TUNNEL_TRAVERSED:
                case CC_EVENT_GOBLIN_CULT_RALLIED:
                case CC_EVENT_GOBLIN_DRAGON_SEED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_WAR: /* war: courts, couriers, and supply lines */
            switch (kind) {
                case CC_EVENT_WAR_PRESSURE:
                case CC_EVENT_WAR_DECLARED:
                case CC_EVENT_PEACE_DECLARED:
                case CC_EVENT_ALLIANCE_DECLARED:
                case CC_EVENT_WAR_CHEST_FUNDED:
                case CC_EVENT_WAR_SUPPLY_BOUGHT:
                case CC_EVENT_WAR_SUPPLY_SHORTAGE:
                case CC_EVENT_WAR_MATERIEL_LOST:
                case CC_EVENT_COURIER_DEPARTED:
                case CC_EVENT_COURIER_ARRIVED:
                case CC_EVENT_COURIER_LOST:
                case CC_EVENT_COURIER_DISTORTED:
                case CC_EVENT_DRAGON_MUSTERED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_THRONE: /* throne: succession, pretenders, and the court's legitimacy */
            switch (kind) {
                case CC_EVENT_KINGDOM_ACTION:
                case CC_EVENT_PRETENDER_CRISIS:
                case CC_EVENT_ROYAL_SUCCESSION:
                case CC_EVENT_MONASTIC_SUCCESSION:
                case CC_EVENT_KING_ANOINTED:
                case CC_EVENT_FACTION_SHIFT:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_WHEAT: /* wheat: the food supply that keeps hunger from the wall */
            switch (kind) {
                case CC_EVENT_HARVEST_FAILED:
                case CC_EVENT_SHORTAGE:
                case CC_EVENT_RELIEF:
                case CC_EVENT_BAKERY_PRODUCTION:
                case CC_EVENT_PAPER_MILLED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_HERDS: /* herds: cows, sheep, and the carriage team */
            switch (kind) {
                case CC_EVENT_COW_CALVING:
                case CC_EVENT_COW_SLAUGHTERED:
                case CC_EVENT_HORSE_BRED:
                case CC_EVENT_FOAL_BORN:
                case CC_EVENT_SHEEP_BRED:
                case CC_EVENT_SHEEP_SHEARED:
                case CC_EVENT_SHEEP_SLAUGHTERED:
                case CC_EVENT_HORSE_TEAM_CHANGED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_PONIES:
            /* Reserved for accounts about the seven companions. */
            return false;
        case CC_GOSSIP_TOPIC_ROAD: /* road: routes, carriage, and the working sites */
            switch (kind) {
                case CC_EVENT_ROUTE_CLOSED:
                case CC_EVENT_ROUTE_REPAIRED:
                case CC_EVENT_ROUTE_DECAY:
                case CC_EVENT_ROYAL_CARRIAGE_BLOCKED:
                case CC_EVENT_ROYAL_CARRIAGE_REROUTED:
                case CC_EVENT_ROAD_HOUSE_LODGING:
                case CC_EVENT_JOURNEY_WARNING:
                case CC_EVENT_WOODLOT_HARVEST:
                case CC_EVENT_QUARRY_OUTPUT:
                case CC_EVENT_MASONRY_REPAIR:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_BANDIT: /* bandit: raiders and the armed road */
            switch (kind) {
                case CC_EVENT_BANDIT_PRESSURE:
                case CC_EVENT_BANDIT_RAID_DEPARTED:
                case CC_EVENT_SETTLEMENT_RAIDED:
                case CC_EVENT_BANDIT_RAID_RETURNED:
                case CC_EVENT_ENCOUNTER_LOOT:
                case CC_EVENT_AMBUSH_EVADED:
                case CC_EVENT_ENCOUNTER_WITHDRAWN:
                case CC_EVENT_HOARD_HEIST_DEPARTED:
                case CC_EVENT_HOARD_HEIST_RETURNED:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_TREASURE: /* treasure: the hoard, the ledger, and wealth's movement */
            switch (kind) {
                case CC_EVENT_TREASURE_CRAFTED:
                case CC_EVENT_DRAGON_HOARD_STOLEN:
                case CC_EVENT_DRAGON_TREASURE_RETURNED:
                case CC_EVENT_DRAGON_HOARD_RECOVERED:
                case CC_EVENT_IRON_LEDGER_LOAN:
                case CC_EVENT_IRON_LEDGER_REPAID:
                case CC_EVENT_INEQUALITY_PRESSURE:
                case CC_EVENT_GOBLIN_TRADE:
                    return true;
                default:
                    return false;
            }
        case CC_GOSSIP_TOPIC_ALL:
            return true;
        default:
            return false;
    }
}

const char *CcGossipTopicName(CcGossipTopic topic)
{
    if (topic < CC_GOSSIP_TOPIC_ALL || topic >= CC_GOSSIP_TOPIC_COUNT)
        return "unknown";
    return TopicNames[topic];
}

bool CcGossipTopicParse(const char *name, CcGossipTopic *topic)
{
    if (name == NULL || topic == NULL) return false;
    for (int32_t i = 0; i < CC_GOSSIP_TOPIC_COUNT; ++i) {
        if (strcmp(name, TopicNames[i]) == 0) {
            *topic = (CcGossipTopic)i;
            return true;
        }
    }
    return false;
}
