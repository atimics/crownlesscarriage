#ifndef CROWNLESS_KNOWN_PRICES_H
#define CROWNLESS_KNOWN_PRICES_H

#include "sim/cc_sim.h"

#include <stddef.h>

/* Prices under the fog (docs/design/town-economies.md).

   The company knows a town's prices only as of the day it last saw them.
   Standing in a town, it sees today's prices. Anywhere else it sees the
   snapshot The Return took when the company last left that town
   (CcTownSeen), labelled with its age. A town the company has never left has
   no known prices.

   Stories the company was told, and news met on the road (CcTownSeen
   road_news: a traveller's telling or a famine notice), can add news newer
   than the snapshot, such as a shortage. News does not invent a number: the price stays the snapshot and
   the news is shown beside it, with the teller's lower confidence.

   Everything here is pure. It reads the simulation and writes only its
   output, so it adds no saved state and cannot change a journal. */

/* Known prices lose confidence over this many days, down to the floor. */
#define CC_KNOWN_PRICE_FADE_DAYS 120
#define CC_KNOWN_PRICE_MIN_CONFIDENCE 10

typedef enum CcKnownPriceSource {
    CC_KNOWN_PRICE_UNKNOWN = 0,
    CC_KNOWN_PRICE_HERE, /* the company is in the town today */
    CC_KNOWN_PRICE_SEEN  /* the company's last view, when it left the town */
} CcKnownPriceSource;

typedef enum CcKnownPriceNews {
    CC_KNOWN_PRICE_NEWS_NONE = 0,
    CC_KNOWN_PRICE_NEWS_SHORTAGE, /* a shortage or failed harvest */
    CC_KNOWN_PRICE_NEWS_SUPPLY    /* relief or a shipment arrived */
} CcKnownPriceNews;

typedef struct CcKnownPrices {
    CcId settlement_id;
    CcKnownPriceSource source;
    int32_t known_day;
    int32_t age_days;
    int32_t confidence; /* 0..100; fades with age */
    int32_t price[CC_GOOD_COUNT];
    int32_t stock[CC_GOOD_COUNT];
    /* The newest story the company was told about the town's food since the
       snapshot. Only set for CC_KNOWN_PRICE_SEEN. */
    CcKnownPriceNews news;
    int32_t news_day;
    int32_t news_confidence;
    CcId news_teller_id;
} CcKnownPrices;

/* What the company knows of a town's prices. Returns false, with source
   CC_KNOWN_PRICE_UNKNOWN and every price zero, for a town it has not seen. */
bool CcKnownPricesFor(const CcSim *sim, CcId settlement_id,
                      CcKnownPrices *known);

/* "today", "1 day ago", "88 days ago". */
void CcKnownPriceAgeText(int32_t age_days, char *text, size_t capacity);

/* One good as a label: "Bread 5c · 88 days ago", "Bread 5c · today" or
   "Bread: no price known". */
void CcKnownPriceLabel(const CcKnownPrices *known, CcGood good,
                       char *text, size_t capacity);

/* A few words for the news: "heard of a shortage", or "" for none. */
const char *CcKnownPriceNewsText(CcKnownPriceNews news);

#endif
