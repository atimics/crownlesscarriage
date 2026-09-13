#include "persistence/cc_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static CcSim sim;
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        FILE *file = fopen(argv[i], "rb");
        if (file == NULL || fseek(file, 0, SEEK_END) != 0) return 2;
        long length = ftell(file);
        if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) return 3;
        unsigned char *bytes = malloc((size_t)length);
        if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length) return 4;
        fclose(file);
        char error[256] = "";
        bool ok = CcSaveDecode(bytes, (size_t)length, &sim, error, sizeof(error));
        free(bytes);
        if (!ok) { fprintf(stderr, "%s: %s\n", argv[i], error); return 5; }
        printf("%s %u %u %016" PRIx64, argv[i], sim.schema_version,
               sim.generator_version, CcSimHash(&sim));
        CcSimAdvanceDays(&sim, 7);
        if (!CcSimValidate(&sim, error, sizeof(error))) {
            fprintf(stderr, "%s after advance: %s\n", argv[i], error);
            return 6;
        }
        printf(" %016" PRIx64 "\n", CcSimHash(&sim));
    }
    return 0;
}
