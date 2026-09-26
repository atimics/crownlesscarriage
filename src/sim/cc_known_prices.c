#include "sim/cc_known_prices.h"

#include "sim/cc_return.h"

#include <stdio.h>

static CcKnownPriceNews NewsForKind(CcEventKind kind)
{
    switch (kind) {
    case CC_EVENT_SHORTAGE:
    case CC_EVENT_HARVEST_FAILED:
        return CC_KNOWN_PRICE_NEWS_SHORTAGE;
    case CC_EVENT_RELIEF:
    case CC_EVENT_SHIPMENT_ARRIVED:
        return CC_KNOWN_PRICE_NEWS_SUPPLY;
    default:
        return CC_KNOWN_PRICE_NEWS_NONE;
    }
}

static CcKnownPriceNews NewsForChange(int32_t kind)
{
    switch (kind) {
    case CC_RETURN_CHANGE_HUNGER:
    case CC_RETURN_CHANGE_STALL_EMPTY:
        return CC_KNOWN_PRICE_NEWS_SHORTAGE;
    case CC_RETURN_CHANGE_FED:
    case CC_RETURN_CHANGE_STALL_RESTOCKED:
        return CC_KNOWN_PRICE_NEWS_SUPPLY;
    default:
        return CC_KNOWN_PRICE_NEWS_NONE;
    }
}

/* News met on the road since the snapshot (The Return, milestone 4): a
   famine notice read at a milestone, or a traveller's telling. A newer piece
   wins; on the same day the more certain one wins. */
static void FindRoadNews(const CcTownSeen *seen, CcKnownPrices *known)
{
    for (int32_t i = 0; i < CC_RETURN_ROAD_NEWS; ++i) {
        const CcRoadNews *piece = &seen->road_news[i];
        CcKnownPriceNews news = NewsForChange(piece->kind);
        if (piece->channel == 0 || news == CC_KNOWN_PRICE_NEWS_NONE ||
            piece->day < known->known_day) continue;
        if (known->news != CC_KNOWN_PRICE_NEWS_NONE &&
            (piece->day < known->news_day ||
             (piece->day == known->news_day &&
              piece->confidence <= known->news_confidence))) continue;
        known->news = news;
        known->news_day = piece->day;
        known->news_teller_id = piece->source_id;
        known->news_confidence = piece->confidence < 0 ? 0 :
            piece->confidence > 100 ? 100 : piece->confidence;
    }
}

/* The newest food story about the town that someone told the company after
   the snapshot. Ties go to the lower slot, then the more certain teller, so
   the result is stable. */
static void FindNews(const CcSim *sim, CcKnownPrices *known)
{
    int32_t carriers = CcSimGossipCarrierCapacity(sim);
    for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
        const CcGossip *story = &sim->gossip[slot];
        CcKnownPriceNews news = NewsForKind(story->kind);
        if (story->event_id == 0U || news == CC_KNOWN_PRICE_NEWS_NONE ||
            story->origin_id != known->settlement_id ||
            story->day <= known->known_day ||
            story->day > sim->current_day) continue;
        if (known->news != CC_KNOWN_PRICE_NEWS_NONE &&
            story->day <= known->news_day) continue;
        uint32_t bit = UINT32_C(1) << (uint32_t)slot;
        CcId teller = 0U;
        int32_t confidence = -1;
        for (int32_t c = 0; c < carriers; ++c) {
            const CcGossipCarrier *carrier = &sim->gossip_carriers[c];
            if (carrier->id == 0U || (carrier->told_player & bit) == 0U)
                continue;
            if (carrier->versions[slot].confidence > confidence) {
                confidence = carrier->versions[slot].confidence;
                teller = carrier->id;
            }
        }
        if (teller == 0U) continue;
        known->news = news;
        known->news_day = story->day;
        known->news_teller_id = teller;
        known->news_confidence = confidence < 0 ? 0 :
            confidence > 100 ? 100 : confidence;
    }
}

bool CcKnownPricesFor(const CcSim *sim, CcId settlement_id,
                      CcKnownPrices *known)
{
    if (known == NULL) return false;
    *known = (CcKnownPrices){.settlement_id = settlement_id};
    if (sim == NULL) return false;
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (place == NULL) return false;
    /* Only the town the company stands in shows today's prices. */
    if (!sim->journey.active && sim->player.location_id == place->id) {
        known->source = CC_KNOWN_PRICE_HERE;
        known->known_day = sim->current_day;
        known->confidence = 100;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            known->price[good] = place->price[good];
            known->stock[good] = place->stock[good];
        }
        return true;
    }
    const CcTownSeen *seen = CcReturnLastSeen(sim, place->id);
    if (seen == NULL) return false;
    known->source = CC_KNOWN_PRICE_SEEN;
    known->known_day = seen->seen_day;
    known->age_days = sim->current_day > seen->seen_day ?
        sim->current_day - seen->seen_day : 0;
    int32_t fade = known->age_days >= CC_KNOWN_PRICE_FADE_DAYS ? 100 :
        known->age_days * 100 / CC_KNOWN_PRICE_FADE_DAYS;
    known->confidence = 100 - fade;
    if (known->confidence < CC_KNOWN_PRICE_MIN_CONFIDENCE)
        known->confidence = CC_KNOWN_PRICE_MIN_CONFIDENCE;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        known->price[good] = seen->price[good];
        known->stock[good] = seen->stock[good];
    }
    FindNews(sim, known);
    FindRoadNews(seen, known);
    return true;
}

void CcKnownPriceAgeText(int32_t age_days, char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return;
    if (age_days <= 0) (void)snprintf(text, capacity, "today");
    else if (age_days == 1) (void)snprintf(text, capacity, "1 day ago");
    else (void)snprintf(text, capacity, "%d days ago", (int)age_days);
}

void CcKnownPriceLabel(const CcKnownPrices *known, CcGood good,
                       char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return;
    const char *name = good >= 0 && good < CC_GOOD_COUNT ?
        CcGoodName(good) : "Goods";
    if (known == NULL || known->source == CC_KNOWN_PRICE_UNKNOWN ||
        good < 0 || good >= CC_GOOD_COUNT) {
        (void)snprintf(text, capacity, "%s: no price known", name);
        return;
    }
    char age[32];
    CcKnownPriceAgeText(known->age_days, age, sizeof(age));
    (void)snprintf(text, capacity, "%s %dc \xC2\xB7 %s", name,
                   (int)known->price[good], age);
}

const char *CcKnownPriceNewsText(CcKnownPriceNews news)
{
    switch (news) {
    case CC_KNOWN_PRICE_NEWS_SHORTAGE: return "heard of a shortage";
    case CC_KNOWN_PRICE_NEWS_SUPPLY: return "heard supplies arrived";
    case CC_KNOWN_PRICE_NEWS_NONE: break;
    }
    return "";
}
