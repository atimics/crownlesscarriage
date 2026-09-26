#include "sim/cc_known_prices.h"
#include "sim/cc_return.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <string.h>

/* Prices under the fog: the company sees today's prices only where it stands.
   Elsewhere it sees the last-seen snapshot with its age, and nothing for a
   town it has never left. */

static CcSim sim;
static char error[256];

static void Check(bool ok)
{
    if (!ok) (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(ok);
}

static CcId TownByName(const CcSim *world, const char *name)
{
    for (int32_t i = 0; i < world->settlement_count; ++i)
        if (strcmp(world->settlements[i].name, name) == 0)
            return world->settlements[i].id;
    CC_CHECK(false);
    return 0U;
}

static CcSettlement *Town(CcSim *world, CcId id)
{
    return CcSimSettlementMutable(world, id);
}

static void Ride(CcSim *world, CcId destination)
{
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = destination};
    Check(CcSimApply(world, &travel, error, sizeof(error)));
    world->journey.ambush_pending = false;
    world->pony_company.encounter = -1;
    for (int32_t step = 0; step < 200000 && world->journey.active; ++step) {
        if (world->journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(world, CC_WORLD_TICKS_PER_SECOND);
        else
            Check(CcTestContinueJourneyPause(world, error, sizeof(error)));
    }
    CC_CHECK(!world->journey.active);
    CC_CHECK(world->player.location_id == destination);
}

static void CheckUnknownAndHere(void)
{
    CcSimInit(&sim, 11U);
    CcId thornford = TownByName(&sim, "Thornford");
    CcId gloamgate = TownByName(&sim, "Gloamgate");
    CC_CHECK(sim.player.location_id == thornford);

    /* Never left, never seen: no price at all. */
    CcKnownPrices known;
    CC_CHECK(!CcKnownPricesFor(&sim, gloamgate, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_UNKNOWN);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
        CC_CHECK(known.price[good] == 0 && known.stock[good] == 0);
    char label[64];
    CcKnownPriceLabel(&known, CC_GOOD_BREAD, label, sizeof(label));
    CC_CHECK(strcmp(label, "Bread: no price known") == 0);

    /* The town the company stands in shows today's prices, and follows them. */
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_HERE);
    CC_CHECK(known.age_days == 0 && known.confidence == 100);
    CC_CHECK(known.known_day == sim.current_day);
    CcSettlement *here = Town(&sim, thornford);
    CC_CHECK(known.price[CC_GOOD_BREAD] == here->price[CC_GOOD_BREAD]);
    here->price[CC_GOOD_BREAD] += 3;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.price[CC_GOOD_BREAD] == here->price[CC_GOOD_BREAD]);
    CcKnownPriceLabel(&known, CC_GOOD_BREAD, label, sizeof(label));
    char want[64];
    (void)snprintf(want, sizeof(want), "Bread %dc \xC2\xB7 today",
                   (int)here->price[CC_GOOD_BREAD]);
    CC_CHECK(strcmp(label, want) == 0);
    here->price[CC_GOOD_BREAD] -= 3;

    /* On the road no town is "here", not even the one just left. */
    sim.journey.active = true;
    CC_CHECK(!CcKnownPricesFor(&sim, thornford, &known));
    sim.journey.active = false;
}

static void CheckRemoteIsNeverLive(void)
{
    CcSimInit(&sim, 11U);
    CcId thornford = TownByName(&sim, "Thornford");
    CcId gloamgate = TownByName(&sim, "Gloamgate");
    int32_t left_bread = Town(&sim, thornford)->price[CC_GOOD_BREAD];
    int32_t left_wheat = Town(&sim, thornford)->price[CC_GOOD_WHEAT];
    Ride(&sim, gloamgate);
    const CcTownSeen *seen = CcReturnLastSeen(&sim, thornford);
    CC_CHECK(seen != NULL);

    CcKnownPrices known;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_SEEN);
    CC_CHECK(known.known_day == seen->seen_day);
    CC_CHECK(known.age_days == sim.current_day - seen->seen_day);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
        CC_CHECK(known.price[good] == seen->price[good]);
    CC_CHECK(known.price[CC_GOOD_BREAD] == left_bread);
    CC_CHECK(known.price[CC_GOOD_WHEAT] == left_wheat);

    /* The live market moves; what the company knows does not. */
    uint64_t hash = CcSimHash(&sim);
    Town(&sim, thornford)->price[CC_GOOD_BREAD] = left_bread + 7;
    Town(&sim, thornford)->stock[CC_GOOD_BREAD] += 40;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.price[CC_GOOD_BREAD] == left_bread);
    CC_CHECK(known.stock[CC_GOOD_BREAD] == seen->stock[CC_GOOD_BREAD]);
    Town(&sim, thornford)->price[CC_GOOD_BREAD] = left_bread;
    Town(&sim, thornford)->stock[CC_GOOD_BREAD] -= 40;
    /* Reading knowledge is pure. */
    CC_CHECK(CcSimHash(&sim) == hash);

    /* Gloamgate, where the company stands, is live. */
    CC_CHECK(CcKnownPricesFor(&sim, gloamgate, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_HERE);

    /* Age labels, and the fade with age. */
    int32_t today = sim.current_day;
    sim.current_day = seen->seen_day + 88;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.age_days == 88);
    CC_CHECK(known.confidence == 100 - 88 * 100 / CC_KNOWN_PRICE_FADE_DAYS);
    char label[64], want[64];
    CcKnownPriceLabel(&known, CC_GOOD_BREAD, label, sizeof(label));
    (void)snprintf(want, sizeof(want), "Bread %dc \xC2\xB7 88 days ago",
                   (int)left_bread);
    CC_CHECK(strcmp(label, want) == 0);
    sim.current_day = seen->seen_day + 1;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CcKnownPriceLabel(&known, CC_GOOD_BREAD, label, sizeof(label));
    (void)snprintf(want, sizeof(want), "Bread %dc \xC2\xB7 1 day ago",
                   (int)left_bread);
    CC_CHECK(strcmp(label, want) == 0);
    sim.current_day = seen->seen_day + 1000;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.confidence == CC_KNOWN_PRICE_MIN_CONFIDENCE);
    sim.current_day = today;

    char age[32];
    CcKnownPriceAgeText(0, age, sizeof(age));
    CC_CHECK(strcmp(age, "today") == 0);
    CcKnownPriceAgeText(2, age, sizeof(age));
    CC_CHECK(strcmp(age, "2 days ago") == 0);

    /* Back in Thornford, the price is live again. */
    Ride(&sim, thornford);
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_HERE);
    CC_CHECK(known.price[CC_GOOD_BREAD] ==
             Town(&sim, thornford)->price[CC_GOOD_BREAD]);
    /* And Gloamgate is now the remembered town. */
    CC_CHECK(CcKnownPricesFor(&sim, gloamgate, &known));
    CC_CHECK(known.source == CC_KNOWN_PRICE_SEEN);
}

static void CheckToldNews(void)
{
    CcSimInit(&sim, 11U);
    CcId thornford = TownByName(&sim, "Thornford");
    Ride(&sim, TownByName(&sim, "Gloamgate"));
    const CcTownSeen *seen = CcReturnLastSeen(&sim, thornford);
    CC_CHECK(seen != NULL);
    memset(sim.gossip, 0, sizeof(sim.gossip));
    for (int32_t i = 0; i < CC_MAX_GOSSIP_CARRIERS; ++i) {
        sim.gossip_carriers[i].stories = 0U;
        sim.gossip_carriers[i].told_player = 0U;
    }
    sim.current_day = seen->seen_day + 30;

    const int32_t slot = 4;
    sim.gossip[slot] = (CcGossip){
        .event_id = CcMakeId(CC_ENTITY_EVENT, 910001U),
        .origin_id = thornford, .day = seen->seen_day + 10,
        .kind = CC_EVENT_SHORTAGE
    };
    CcGossipCarrier *carrier = &sim.gossip_carriers[2];
    carrier->id = sim.characters[3].id;
    carrier->stories = UINT32_C(1) << (uint32_t)slot;
    carrier->versions[slot].confidence = 55;

    /* A story nobody told the company adds nothing. */
    CcKnownPrices known;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.news == CC_KNOWN_PRICE_NEWS_NONE);

    /* Told: the news shows with the teller's confidence; the price stays. */
    carrier->told_player = UINT32_C(1) << (uint32_t)slot;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.news == CC_KNOWN_PRICE_NEWS_SHORTAGE);
    CC_CHECK(known.news_day == seen->seen_day + 10);
    CC_CHECK(known.news_confidence == 55);
    CC_CHECK(known.news_teller_id == carrier->id);
    CC_CHECK(known.news_confidence < 100);
    CC_CHECK(known.price[CC_GOOD_BREAD] == seen->price[CC_GOOD_BREAD]);
    CC_CHECK(strcmp(CcKnownPriceNewsText(known.news), "heard of a shortage") == 0);

    /* Older than the snapshot: the company already saw the town since. */
    sim.gossip[slot].day = seen->seen_day;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.news == CC_KNOWN_PRICE_NEWS_NONE);

    /* A newer relief story replaces the shortage. */
    sim.gossip[slot].day = seen->seen_day + 10;
    sim.gossip[slot + 1] = (CcGossip){
        .event_id = CcMakeId(CC_ENTITY_EVENT, 910002U),
        .origin_id = thornford, .day = seen->seen_day + 20,
        .kind = CC_EVENT_RELIEF
    };
    carrier->stories |= UINT32_C(1) << (uint32_t)(slot + 1);
    carrier->told_player |= UINT32_C(1) << (uint32_t)(slot + 1);
    carrier->versions[slot + 1].confidence = 40;
    CC_CHECK(CcKnownPricesFor(&sim, thornford, &known));
    CC_CHECK(known.news == CC_KNOWN_PRICE_NEWS_SUPPLY);
    CC_CHECK(known.news_confidence == 40);

    /* News never reveals a town the company has not seen. */
    CcId silverwick = TownByName(&sim, "Silverwick");
    sim.gossip[slot].origin_id = silverwick;
    CC_CHECK(!CcKnownPricesFor(&sim, silverwick, &known));
    CC_CHECK(known.news == CC_KNOWN_PRICE_NEWS_NONE);
}

int main(void)
{
    CheckUnknownAndHere();
    CheckRemoteIsNeverLive();
    CheckToldNews();
    (void)printf("known prices tests passed\n");
    return 0;
}
