#include "sim/cc_sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void Check(uint32_t schema, uint32_t generator)
{
#ifdef CC_QUERY_AVAILABLE
    bool supported = CcSimSupportsVersions(schema, generator);
#else
    static CcSim sim;
    sim.schema_version = schema;
    sim.generator_version = generator;
    char error[256] = "";
    if (CcSimValidate(&sim, error, sizeof(error))) exit(2);
    bool supported = strcmp(error, "Simulation clock or identity state is invalid.") == 0;
    if (!supported && strcmp(error, "Simulation version is unsupported.") != 0) exit(3);
#endif
    printf("%u %u %d\n", schema, generator, supported);
}
int main(void)
{
    for (uint32_t schema = 0; schema <= 80; ++schema)
        for (uint32_t generator = 0; generator <= 32; ++generator)
            Check(schema, generator);
    Check(UINT32_MAX, CC_GENERATOR_VERSION);
    Check(CC_SIM_SCHEMA_VERSION, UINT32_MAX);
    Check(UINT32_MAX, UINT32_MAX);
    return 0;
}
