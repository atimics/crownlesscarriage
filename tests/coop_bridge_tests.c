#include "multiplayer/cc_coop.h"
#include "test_support.h"
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char error[256];
    CcSim *first = CcCoopCreate(UINT32_C(0xc0a71a9e));
    CcSim *second = CcCoopCreate(42U);
    CC_CHECK(first != NULL && second != NULL);
    uint64_t initial = CcSimHash(first);
    CC_CHECK(!CcCoopApply(first, "trade", 0U, CC_GOOD_BREAD, 1000000, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == initial);
    CC_CHECK(!CcCoopApply(first, "advance", 0U, 0, 1, error, sizeof(error)));
    CC_CHECK(!CcCoopApply(first, "fight", 0U, 0, 1, error, sizeof(error)));
    CC_CHECK(CcCoopApply(first, "trade", 0U, CC_GOOD_BREAD, 1, error, sizeof(error)));
    unsigned char *bytes = NULL;
    size_t length = 0U;
    CC_CHECK(CcCoopEncode(first, &bytes, &length, error, sizeof(error)));
    CC_CHECK(length > 100U);
    CC_CHECK(CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    bytes[0] = 'X';
    CC_CHECK(!CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    CcCoopFree(bytes);
    char *json = malloc(CC_COOP_JSON_CAPACITY);
    CC_CHECK(json != NULL);
    CC_CHECK(CcCoopSnapshot(first, json, CC_COOP_JSON_CAPACITY));
    CC_CHECK(strstr(json, "\"company\":{") != NULL);
    CC_CHECK(!CcCoopSnapshot(first, json, 2U));
    free(json);
    CC_CHECK(!CcCoopAdvance(first, -1, error, sizeof(error)));
    CC_CHECK(!CcCoopAdvance(first, 3601, error, sizeof(error)));
    initial = CcSimHash(first);
    CC_CHECK(!CcCoopAdvanceAway(first, -1, error, sizeof(error)));
    CC_CHECK(!CcCoopAdvanceAway(first, 366, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == initial);
    int32_t day = first->current_day;
    CcId location = first->player.location_id;
    for (int year = 0; year < 100; ++year)
        CC_CHECK(CcCoopAdvanceAway(first, 365, error, sizeof(error)));
    CC_CHECK(first->current_day == day + 36500);
    CC_CHECK(first->player.location_id == location);
    CC_CHECK(CcCoopEncode(first, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(second, bytes, length, error, sizeof(error)));
    CC_CHECK(CcSimHash(first) == CcSimHash(second));
    CcCoopFree(bytes);
    CcCoopDestroy(first);
    CcCoopDestroy(second);
    return 0;
}
