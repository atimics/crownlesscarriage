#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int CompareName(const void *a, const void *b) { return strcmp(a, b); }

static void CheckNamePool(void)
{
    static char names[4096][CC_NAME_CAPACITY];
    static const char *local_families[2][16] = {
        {"Fordward", "Thornbank", "Shallowford", "Miller", "Millward", "Millrace", "Rillbank", "Underwheel",
         "Sheafbinder", "Barleycroft", "Hedgerow", "Longmead", "Drover", "Cartwright", "Oxley", "Wainward"},
        {"Deepwell", "Shaftward", "Underwick", "Silvervein", "Furnace", "Cinderhand", "Bellows", "Emberwright",
         "Stonecutter", "Flintbeck", "Stonehewer", "Slagfield", "Lampwright", "Wickman", "Windlass", "Orehauler"}
    };
    for (int32_t place = CC_SETTLEMENT_FARMING; place <= CC_SETTLEMENT_DUNGEON_TOWN; ++place) {
        int local = 0;
        for (uint32_t ordinal = 0; ordinal < 4096U; ++ordinal) {
            struct { char name[CC_NAME_CAPACITY]; unsigned char guard; } sample;
            memset(&sample, 0xa5, sizeof(sample));
            CcGenerateSettlementCharacterName(42U, CcMakeId(CC_ENTITY_SETTLEMENT, 7U),
                place, 3, ordinal, sample.name);
            CC_CHECK(sample.guard == 0xa5);
            CC_CHECK(memchr(sample.name, '\0', CC_NAME_CAPACITY) != NULL);
            const char *family = strchr(sample.name, ' ');
            CC_CHECK(family != NULL && family != sample.name && family[1] != '\0');
            CC_CHECK(strlen(sample.name) < CC_NAME_CAPACITY - 1U);
            CcGenerateSettlementCharacterName(42U, CcMakeId(CC_ENTITY_SETTLEMENT, 7U),
                place, 3, ordinal, names[ordinal]);
            CC_CHECK(strcmp(sample.name, names[ordinal]) == 0);
            if (place < 2) for (int i = 0; i < 16; ++i)
                if (strcmp(family + 1, local_families[place][i]) == 0) ++local;
        }
        if (place < 2) CC_CHECK(local > 2900 && local < 3500);
        qsort(names, 4096U, sizeof(names[0]), CompareName);
        int unique = 1;
        for (int i = 1; i < 4096; ++i) if (strcmp(names[i-1], names[i]) != 0) ++unique;
        CC_CHECK(unique > 1000);
        printf("%s: %d distinct names in 4096 samples\n", CcSettlementFunctionName((CcSettlementFunction)place), unique);
    }
    char invalid[CC_NAME_CAPACITY], repeated[CC_NAME_CAPACITY];
    CcGenerateSettlementCharacterName(42U, 0U, -1, 0, 0U, invalid);
    CcGenerateSettlementCharacterName(42U, 0U, INT32_MAX, 0, 0U, repeated);
    CC_CHECK(strcmp(invalid, repeated) == 0);
    CcGenerateSettlementCharacterName(42U, 0U, 0, 0, 0U, NULL);
}

static void CheckResidentsAndSaves(void)
{
    static CcSim first, repeated, restored;
    char error[256], path[1024];
    for (uint32_t seed = 1U; seed <= 8U; ++seed) {
        CcSimInit(&first, seed);
        CcSimInit(&repeated, seed);
        CC_CHECK(CcSimHash(&first) == CcSimHash(&repeated));
        CC_CHECK(first.character_count == CC_MAX_CHARACTERS);
        int rooted = 0;
        for (int i = 0; i < first.character_count; ++i) {
            const CcCharacter *person = &first.characters[i];
            const CcSettlement *home = CcSimSettlement(&first, person->home_settlement_id);
            CC_CHECK(home != NULL);
            for (uint32_t ordinal = 0; ordinal < 64U; ++ordinal) {
                char name[CC_NAME_CAPACITY];
                CcGenerateSettlementCharacterName(seed, home->id, (int32_t)home->function, 0, ordinal, name);
                if (strcmp(name, person->name) == 0) { ++rooted; break; }
            }
            first.characters[i].death_day = first.current_day + 1;
        }
        CC_CHECK(rooted >= 12);
        repeated = first;
        CcSimAdvanceDays(&first, 1);
        CC_CHECK(first.character_births == CC_MAX_CHARACTERS);
        for (int i = 0; i < first.character_count; ++i) {
            CC_CHECK(strcmp(strrchr(first.characters[i].name, ' '), strrchr(repeated.characters[i].name, ' ')) == 0);
        }
        CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
        unsigned char *bytes = NULL; size_t length = 0;
        CC_CHECK(CcSaveEncode(&first, &bytes, &length, error, sizeof(error)));
        CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
        CcSaveFreeBuffer(bytes);
        CC_CHECK(CcSimHash(&first) == CcSimHash(&restored));
        CcSimAdvanceDays(&first, 3650);
        CcSimAdvanceDays(&restored, 3650);
        CC_CHECK(CcSimHash(&first) == CcSimHash(&restored));
        CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    }
    (void)snprintf(path, sizeof(path), "%s/tests/fixtures/shipped/schema-57-generator-25-names-journal.ccsave", CC_TEST_SOURCE_DIR);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.character_births == 24);
    CC_CHECK(strcmp(restored.characters[0].name, "Ilya Venn") == 0);
    CC_CHECK(strcmp(restored.characters[23].name, "Tamsin Lark") == 0);
    restored.schema_version = 57U;
    CC_CHECK(CcSimHash(&restored) == UINT64_C(9371884615631308597));
}

int main(void) { CheckNamePool(); CheckResidentsAndSaves(); return 0; }
