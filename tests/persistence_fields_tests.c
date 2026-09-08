#include "persistence/cc_save.h"
#include "test_support.h"

#include <string.h>

static CcSim baseline;
static CcSim changed;
static CcSim restored;
static unsigned checks;

static void RoundTrip(const char *field)
{
    char error[256];
    if (!CcSimValidate(&changed, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", field, error);
        CC_CHECK(false);
    }
    if (CcSimHash(&changed) == CcSimHash(&baseline)) {
        fprintf(stderr, "Hash omitted %s\n", field);
        CC_CHECK(false);
    }
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&changed, &bytes, &length, error, sizeof(error)));
    memset(&restored, 0, sizeof(restored));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    checks++;
}

/* Compare each field directly, independently of the hash implementation.
   Each case starts from the baseline so a second changed field cannot hide
   an omission. Fixed values also exercise fields that start at zero. */
#define CHECK_FIELD(field, value) do { \
    changed = baseline; \
    changed.field = (value); \
    CC_CHECK(changed.field != baseline.field); \
    RoundTrip(#field); \
    if (restored.field != changed.field) { \
        fprintf(stderr, "Save round trip omitted %s\n", #field); \
        CC_CHECK(false); \
    } \
} while (0)

int main(void)
{
    CcSimInit(&baseline, UINT32_C(0x5eed0001));
    for (int32_t town = 0; town < baseline.settlement_count; ++town) {
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            CHECK_FIELD(settlements[town].stock[good],
                        baseline.settlements[town].stock[good] + 1);
            CHECK_FIELD(settlements[town].reserve_target[good],
                        baseline.settlements[town].reserve_target[good] + 1);
            CHECK_FIELD(settlements[town].production[good],
                        baseline.settlements[town].production[good] + 1);
            CHECK_FIELD(settlements[town].consumption[good],
                        baseline.settlements[town].consumption[good] + 1);
            CHECK_FIELD(settlements[town].price[good],
                        baseline.settlements[town].price[good] + 1);
        }
        CHECK_FIELD(settlements[town].farm_tool_wear, 1);
        CHECK_FIELD(settlements[town].mine_tool_wear, 1);
        CHECK_FIELD(settlements[town].smith_tool_wear, 1);
        CHECK_FIELD(settlements[town].paper_tool_wear, 1);
        CHECK_FIELD(settlements[town].pony_adults,
                    baseline.settlements[town].pony_adults + 1);
        CHECK_FIELD(settlements[town].pony_foals,
                    baseline.settlements[town].pony_foals + 1);
        CHECK_FIELD(settlements[town].pony_condition,
                    (baseline.settlements[town].pony_condition + 1) % 101);
        CHECK_FIELD(settlements[town].pony_hunger,
                    baseline.settlements[town].pony_hunger + 1);
    }
    for (int32_t site = 0; site < baseline.road_site_count; ++site) {
        CHECK_FIELD(road_sites[site].condition,
                    baseline.road_sites[site].condition - 1);
        CHECK_FIELD(road_sites[site].side, -baseline.road_sites[site].side);
    }
    CHECK_FIELD(archives.dead_since_day, 1);
    CHECK_FIELD(archives.lore_lost_total, baseline.archives.lore_lost_total + 1);
    CHECK_FIELD(archives.kit_tool_wear, 1);
    printf("Verified %u independent hash and saved-field mutations\n", checks);
    return 0;
}
