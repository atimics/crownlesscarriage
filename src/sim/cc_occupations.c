#include "sim/cc_occupations.h"

static bool Service(const CcSettlement *town, CcServiceKind service)
{
    return (town->service_mask & (UINT32_C(1) << (uint32_t)service)) != 0;
}

const char *CcOccupationName(CcCharacterOccupation occupation)
{
    static const char *const names[CC_OCCUPATION_COUNT] = {
        "none", "woodcutter", "shepherd", "miller", "smith", "quarryman",
        "farmer", "baker", "innkeeper", "cartwright", "scribe"
    };
    return occupation >= CC_OCCUPATION_NONE && occupation < CC_OCCUPATION_COUNT ?
        names[occupation] : "unknown";
}

CcGossipTopic CcOccupationTopic(CcCharacterOccupation occupation)
{
    switch (occupation) {
        case CC_OCCUPATION_SHEPHERD: return CC_GOSSIP_TOPIC_HERDS;
        case CC_OCCUPATION_MILLER:
        case CC_OCCUPATION_FARMER:
        case CC_OCCUPATION_BAKER: return CC_GOSSIP_TOPIC_WHEAT;
        case CC_OCCUPATION_WOODCUTTER:
        case CC_OCCUPATION_QUARRYMAN:
        case CC_OCCUPATION_INNKEEPER:
        case CC_OCCUPATION_CARTWRIGHT: return CC_GOSSIP_TOPIC_ROAD;
        case CC_OCCUPATION_SMITH: return CC_GOSSIP_TOPIC_WAR;
        case CC_OCCUPATION_SCRIBE: return CC_GOSSIP_TOPIC_THRONE;
        default: return CC_GOSSIP_TOPIC_COUNT;
    }
}

bool CcGossipCraftEvent(CcEventKind kind)
{
    if (CcGossipTopicMatches(CC_GOSSIP_TOPIC_HERDS, kind)) return true;
    return kind == CC_EVENT_PAPER_MILLED || kind == CC_EVENT_BAKERY_PRODUCTION ||
        kind == CC_EVENT_WOODLOT_HARVEST || kind == CC_EVENT_QUARRY_OUTPUT ||
        kind == CC_EVENT_MASONRY_REPAIR;
}

bool CcOccupationObserves(CcCharacterOccupation occupation, CcEventKind kind)
{
    return CcGossipTopicMatches(CcOccupationTopic(occupation), kind);
}

CcCharacterOccupation CcSimInitialOccupation(const CcSim *sim, CcId home, CcId person)
{
    if (sim == NULL) return CC_OCCUPATION_NONE;
    const CcSettlement *town = CcSimSettlement(sim, home);
    if (town == NULL) return CC_OCCUPATION_NONE;
    CcCharacterOccupation trades[CC_OCCUPATION_COUNT];
    int32_t count = 0;
    if (Service(town, CC_SERVICE_FARM) || Service(town, CC_SERVICE_STABLE))
        trades[count++] = CC_OCCUPATION_SHEPHERD;
    if (Service(town, CC_SERVICE_FARM)) trades[count++] = CC_OCCUPATION_FARMER;
    if (Service(town, CC_SERVICE_MILL)) trades[count++] = CC_OCCUPATION_MILLER;
    if (Service(town, CC_SERVICE_SMITHY)) trades[count++] = CC_OCCUPATION_SMITH;
    if (Service(town, CC_SERVICE_BAKERY)) trades[count++] = CC_OCCUPATION_BAKER;
    if (Service(town, CC_SERVICE_INN)) trades[count++] = CC_OCCUPATION_INNKEEPER;
    if (Service(town, CC_SERVICE_STABLE)) trades[count++] = CC_OCCUPATION_CARTWRIGHT;
    if (town->production[CC_GOOD_WOOD] > 0) trades[count++] = CC_OCCUPATION_WOODCUTTER;
    if (town->production[CC_GOOD_STONE] > 0) trades[count++] = CC_OCCUPATION_QUARRYMAN;
    trades[count++] = CC_OCCUPATION_SCRIBE;
    int32_t ordinal = 0;
    for (int32_t i = 0; i < sim->character_count; ++i)
        if (sim->characters[i].home_settlement_id == home && sim->characters[i].id < person)
            ++ordinal;
    return trades[ordinal % count];
}

void CcSimInitializeOccupations(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 79U) return;
    for (int32_t i = 0; i < sim->character_count; ++i)
        sim->characters[i].occupation = CcSimInitialOccupation(sim,
            sim->characters[i].home_settlement_id, sim->characters[i].id);
}
