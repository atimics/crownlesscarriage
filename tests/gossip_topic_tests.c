#include "sim/cc_gossip_topics.h"
#include "test_support.h"

#include <string.h>

int main(void)
{
    /* A calf and a shearing belong on the same research page. */
    const CcEventKind herds[] = {
        CC_EVENT_COW_CALVING, CC_EVENT_COW_SLAUGHTERED,
        CC_EVENT_HORSE_BRED, CC_EVENT_FOAL_BORN,
        CC_EVENT_SHEEP_BRED, CC_EVENT_SHEEP_SHEARED,
        CC_EVENT_SHEEP_SLAUGHTERED, CC_EVENT_HORSE_TEAM_CHANGED
    };
    for (size_t i = 0; i < sizeof(herds) / sizeof(herds[0]); ++i) {
        CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_HERDS, herds[i]));
        CC_CHECK(!CcGossipTopicMatches(CC_GOSSIP_TOPIC_WHEAT, herds[i]));
    }
    CC_CHECK(!CcGossipTopicMatches(CC_GOSSIP_TOPIC_HERDS, CC_EVENT_PLAYER_TRADE));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_ROAD, CC_EVENT_QUARRY_OUTPUT));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_ROAD, CC_EVENT_WOODLOT_HARVEST));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_WHEAT, CC_EVENT_PAPER_MILLED));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_WHEAT, CC_EVENT_BAKERY_PRODUCTION));
    /* Overlap lets separate commissions use the same underlying account. */
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_DRAGON, CC_EVENT_DRAGON_MUSTERED));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_WAR, CC_EVENT_DRAGON_MUSTERED));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_GOBLIN, CC_EVENT_GOBLIN_TRADE));
    CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_TREASURE, CC_EVENT_GOBLIN_TRADE));
    for (int32_t kind = CC_EVENT_HARVEST_FAILED; kind <= CC_EVENT_ROAD_SITE_PRODUCTION; ++kind) {
        CC_CHECK(CcGossipTopicMatches(CC_GOSSIP_TOPIC_ALL, (CcEventKind)kind));
        CC_CHECK(!CcGossipTopicMatches(CC_GOSSIP_TOPIC_PONIES, (CcEventKind)kind));
        CC_CHECK(!CcGossipTopicMatches(CC_GOSSIP_TOPIC_COUNT, (CcEventKind)kind));
    }
    for (int32_t i = 0; i < CC_GOSSIP_TOPIC_COUNT; ++i) {
        CcGossipTopic parsed = CC_GOSSIP_TOPIC_COUNT;
        CC_CHECK(CcGossipTopicParse(CcGossipTopicName((CcGossipTopic)i), &parsed));
        CC_CHECK(parsed == (CcGossipTopic)i);
    }
    CcGossipTopic topic = CC_GOSSIP_TOPIC_HERDS;
    CC_CHECK(!CcGossipTopicParse("Herds", &topic));
    CC_CHECK(!CcGossipTopicParse("", &topic));
    CC_CHECK(!CcGossipTopicParse(NULL, &topic));
    CC_CHECK(!CcGossipTopicParse("herds", NULL));
    CC_CHECK(topic == CC_GOSSIP_TOPIC_HERDS);
    CC_CHECK(strcmp(CcGossipTopicName((CcGossipTopic)-1), "unknown") == 0);
    CC_CHECK(strcmp(CcGossipTopicName(CC_GOSSIP_TOPIC_COUNT), "unknown") == 0);
    CC_CHECK(!CcGossipTopicMatches((CcGossipTopic)-1, CC_EVENT_COW_CALVING));
    return 0;
}
