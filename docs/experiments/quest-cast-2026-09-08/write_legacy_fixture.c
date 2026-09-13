/* Compile against the schema 73 persistence and simulation libraries. */
#include "persistence/cc_save.h"
#include <inttypes.h>
#include <stdio.h>
#include <sqlite3.h>
static CcSim sim;
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    CcSimInit(&sim, 42U);
    if (sim.schema_version != 73U) return 3;
    char error[256];
    CcJournal *journal = CcJournalStart(argv[1], &sim, error, sizeof(error));
    if (journal == NULL || !CcJournalAdvanceDays(journal, &sim, 100, error, sizeof(error)) ||
        !CcJournalFlush(journal, &sim, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    printf("%" PRIu64 "\n", CcSimHash(&sim));
    CcJournalAbandon(&journal);
    /* Package one portable database while retaining the action journal. */
    sqlite3 *database = NULL;
    if (sqlite3_open(argv[1], &database) != SQLITE_OK) return 1;
    int result = sqlite3_exec(database, "PRAGMA journal_mode=DELETE;", NULL, NULL, NULL);
    sqlite3_close(database);
    if (result != SQLITE_OK) return 1;
    return 0;
}
