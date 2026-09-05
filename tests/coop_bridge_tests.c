#include "multiplayer/cc_coop.h"
#include "test_support.h"
#include <stdlib.h>
#include <string.h>

static void CheckSharedPonies(void)
{
    char error[256];
    CcSim *host = CcCoopCreate(117U);
    CcSim *guest = CcCoopCreate(42U);
    CC_CHECK(host != NULL && guest != NULL);
    CC_CHECK(CcCoopApply(host, "travel", host->settlements[1].id, 0, 0, error, sizeof(error)));
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        if (host->pony_company.ponies[i].route_id != 0U)
            host->pony_company.ponies[i].route_id = host->journey.route_id;
    }
    for (int32_t tick = 0; tick < 10000 && CcPonyOnRoad(host) < 0; ++tick)
        CC_CHECK(CcCoopAdvance(host, 1, error, sizeof(error)));
    int32_t pony = CcPonyOnRoad(host);
    CC_CHECK(pony >= 0);
    CcId target = (CcId)pony + 1U;
    CcGood good = CcPonyQuestGood(&host->pony_company.ponies[pony]);
    host->player.cargo[good] = host->pony_company.ponies[pony].quest_amount;
    CC_CHECK(CcCoopApply(host, "meet_pony", target, 0, 0, error, sizeof(error)));
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    CC_CHECK(CcCoopApply(guest, "leave_pony", target, 0, 0, error, sizeof(error)));
    CC_CHECK(guest->pony_company.encounter == -1);
    CC_CHECK(CcCoopApply(host, "help_pony", target, 0, 0, error, sizeof(error)));
    int32_t released = host->pony_company.team[1];
    CC_CHECK(CcCoopApply(host, "swap_pony", target, 0, 1, error, sizeof(error)));
    CC_CHECK(host->pony_company.team[1] == pony);
    CC_CHECK(host->pony_company.ponies[released].releases == 1);
    CC_CHECK(CcCoopEncode(host, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcCoopDecode(guest, bytes, length, error, sizeof(error)));
    CcCoopFree(bytes);
    CC_CHECK(CcSimHash(host) == CcSimHash(guest));
    CcCoopDestroy(host);
    CcCoopDestroy(guest);
}

int main(void)
{
    CheckSharedPonies();
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
