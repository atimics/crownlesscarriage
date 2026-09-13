/* Exercise collision and capacity paths through the private ledger. */
#include "sim/cc_identity.c"
#include "test_support.h"
#include <string.h>
static CcIdentityLedger ledger;
int main(void)
{
    char error[128];
    CcId colliding[16];
    int found = 0;
    for (uint64_t serial = 1; found < 16; ++serial) {
        CcId id = CcMakeId(CC_ENTITY_CHARACTER, serial);
        if (IdentityBucket(id) == CC_IDENTITY_BUCKETS - 1U) colliding[found++] = id;
    }
    for (int i = 0; i < 16; ++i)
        CC_CHECK(TrackIdentity(&ledger, colliding[i], CC_ENTITY_CHARACTER, error, sizeof(error)));
    for (int i = 0; i < 16; ++i) {
        CC_CHECK(!TrackIdentity(&ledger, colliding[i], CC_ENTITY_CHARACTER, error, sizeof(error)));
        CC_CHECK(strcmp(error, "Simulation identities are not unique.") == 0);
        CC_CHECK(ledger.count == 16);
    }
    memset(&ledger, 0, sizeof(ledger));
    for (int i = 0; i < CC_MAX_TRACKED_IDENTITIES; ++i)
        CC_CHECK(TrackIdentity(&ledger, CcMakeId(CC_ENTITY_CHARACTER, (uint64_t)i + 1U),
            CC_ENTITY_CHARACTER, error, sizeof(error)));
    CC_CHECK(ledger.count == CC_MAX_TRACKED_IDENTITIES);
    CC_CHECK(ledger.greatest_serial == CC_MAX_TRACKED_IDENTITIES);
    CC_CHECK(!TrackIdentity(&ledger, CcMakeId(CC_ENTITY_CHARACTER, CC_MAX_TRACKED_IDENTITIES + 1U),
        CC_ENTITY_CHARACTER, error, sizeof(error)));
    CC_CHECK(strcmp(error, "Simulation identity is invalid.") == 0);
    CC_CHECK(ledger.count == CC_MAX_TRACKED_IDENTITIES);
    memset(&ledger, 0, sizeof(ledger));
    CC_CHECK(!TrackIdentity(&ledger, 0, CC_ENTITY_CHARACTER, error, sizeof(error)));
    CC_CHECK(!TrackIdentity(&ledger, CcMakeId(CC_ENTITY_ROUTE, 1),
        CC_ENTITY_CHARACTER, error, sizeof(error)));
    CC_CHECK(ledger.count == 0 && ledger.greatest_serial == 0);
    printf("Verified collision chains, wraparound, duplicates, and %d occupied identity slots\n",
        CC_MAX_TRACKED_IDENTITIES);
    return 0;
}
