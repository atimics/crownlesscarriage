#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim sim, loaded;
int main(void)
{
    for (uint32_t schema = 0; schema <= 102U; ++schema)
        for (uint32_t generator = 0; generator <= 30U; ++generator)
            printf("pair %u %u %d\n", schema, generator, CcSimSupportsVersions(schema, generator));
    for (uint32_t schema = 38U; schema <= 98U; ++schema) {
        CcSimInit(&sim, 42U);
        sim.schema_version = schema;
        CcSimAdvanceDays(&sim, 7);
        unsigned char *bytes = NULL; size_t length = 0; char error[256];
        if (!CcSaveEncode(&sim, &bytes, &length, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
        if (!CcSaveDecode(bytes, length, &loaded, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 2; }
        CcSaveFreeBuffer(bytes);
        if (loaded.schema_version != CC_SIM_SCHEMA_VERSION) return 3;
        loaded.schema_version = 98U;
        printf("save %u %016" PRIx64 " %016" PRIx64 "\n", schema, CcSimHash(&sim), CcSimHash(&loaded));
    }
    return 0;
}
