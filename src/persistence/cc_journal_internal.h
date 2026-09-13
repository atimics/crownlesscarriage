#ifndef CROWNLESS_JOURNAL_INTERNAL_H
#define CROWNLESS_JOURNAL_INTERNAL_H

#include "sim/cc_sim.h"
#include <sqlite3.h>

/* Shared record contract for journal writes and historical replay. */
#define CC_JOURNAL_RECORD_VERSION 1
#define CC_JOURNAL_MAX_DAY_ADVANCE 3650
#define CC_JOURNAL_MAX_RUNTIME_ADVANCE 3600

typedef enum CcJournalOperationKind {
    CC_JOURNAL_OPERATION_COMMAND = 1,
    CC_JOURNAL_OPERATION_ADVANCE_DAYS = 2,
    CC_JOURNAL_OPERATION_ADVANCE_RUNTIME_TICKS = 3
} CcJournalOperationKind;

bool CcSaveParseStoredHash(const unsigned char *text, uint64_t *hash);
bool CcJournalReplay(sqlite3 *database, CcSim *sim,
    uint64_t generation, uint64_t cursor, uint64_t *replayed_through,
    char *error, size_t error_capacity);

#endif
