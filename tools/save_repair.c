#include "persistence/cc_save.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        (void)fprintf(stderr, "usage: crownless_save_repair <campaign.ccsave>\n");
        return 2;
    }
    char error[256];
    if (!CcSaveRepairHash(argv[1], error, sizeof(error))) {
        (void)fprintf(stderr, "repair failed: %s\n", error);
        return 1;
    }
    (void)printf("Repaired campaign state hash: %s\n", argv[1]);
    return 0;
}
