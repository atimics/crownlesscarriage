#ifndef CC_GOSSIP_TOPICS_H
#define CC_GOSSIP_TOPICS_H

#include "sim/cc_sim.h"

/* Topics describe the subject of an account. An event can match several
   topics. Ownership and provenance belong to the held account. */
typedef enum CcGossipTopic {
    CC_GOSSIP_TOPIC_ALL = 0,
    CC_GOSSIP_TOPIC_DRAGON = 1,
    CC_GOSSIP_TOPIC_GOBLIN = 2,
    CC_GOSSIP_TOPIC_WAR = 3,
    CC_GOSSIP_TOPIC_THRONE = 4,
    CC_GOSSIP_TOPIC_WHEAT = 5,
    CC_GOSSIP_TOPIC_HERDS = 6,
    CC_GOSSIP_TOPIC_PONIES = 7,
    CC_GOSSIP_TOPIC_ROAD = 8,
    CC_GOSSIP_TOPIC_BANDIT = 9,
    CC_GOSSIP_TOPIC_TREASURE = 10,
    CC_GOSSIP_TOPIC_COUNT
} CcGossipTopic;

/* ALL matches every event kind. Invalid topics return false. */
bool CcGossipTopicMatches(CcGossipTopic topic, CcEventKind kind);
const char *CcGossipTopicName(CcGossipTopic topic);
/* On success, writes the topic. On failure, preserves the caller's value. */
bool CcGossipTopicParse(const char *name, CcGossipTopic *topic);

#endif
