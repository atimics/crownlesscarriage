#include "sim/cc_sim.h"

#include <stddef.h>

/* The save compatibility contract from cc_sim.h, as data. Each row is a
   closed range of schema versions paired with a closed range of generator
   versions that we still accept; a save is loadable when it falls inside any
   row. This replaced ninety lines of chained equality tests that grew by two
   every time either version was bumped.

   Adding a version means editing one row, or adding one. Keep it that way. */
#define CC_OLDEST_SUPPORTED_SCHEMA 2U
#define CC_NEWEST_LEGACY_SCHEMA 78U

typedef struct CcVersionPairing {
    uint32_t schema_low;
    uint32_t schema_high;
    uint32_t generator_low;
    uint32_t generator_high;
} CcVersionPairing;

static const CcVersionPairing CC_SUPPORTED_VERSIONS[] = {
    /* The current pair. */
    { CC_SIM_SCHEMA_VERSION, CC_SIM_SCHEMA_VERSION,
      CC_GENERATOR_VERSION, CC_GENERATOR_VERSION },
    /* Schemas old enough that the current generator still reads them, and the
       run of recent schemas the current generator wrote. Note the gap: 28
       through 31 are deliberately absent, because those schemas only ever
       shipped alongside their own generators, listed below. */
    { 2U, 27U, CC_GENERATOR_VERSION, CC_GENERATOR_VERSION },
    { 32U, 78U, CC_GENERATOR_VERSION, CC_GENERATOR_VERSION },
    /* Schemas pinned to the generator they shipped with. */
    { 31U, 31U, 24U, 24U },
    { 27U, 27U, 21U, 23U },
    { 28U, 28U, 22U, 22U },
    { 29U, 29U, 23U, 23U },
    { 30U, 30U, 23U, 23U },
    /* Preserve the historical generator pairings. Schema 52 shipped with 25. */
    { CC_OLDEST_SUPPORTED_SCHEMA, 51U, 2U, 21U },
};

bool CcSimSupportsVersions(uint32_t schema_version, uint32_t generator_version)
{
    bool legacy_schema =
        schema_version >= CC_OLDEST_SUPPORTED_SCHEMA &&
        schema_version <= CC_NEWEST_LEGACY_SCHEMA;
    bool supported_generator = false;
    for (size_t pairing = 0;
         pairing < sizeof(CC_SUPPORTED_VERSIONS) /
                   sizeof(CC_SUPPORTED_VERSIONS[0]);
         ++pairing) {
        const CcVersionPairing *supported = &CC_SUPPORTED_VERSIONS[pairing];
        if (schema_version >= supported->schema_low &&
            schema_version <= supported->schema_high &&
            generator_version >= supported->generator_low &&
            generator_version <= supported->generator_high) {
            supported_generator = true;
            break;
        }
    }
    return (legacy_schema || schema_version == CC_SIM_SCHEMA_VERSION) &&
           supported_generator;
}
