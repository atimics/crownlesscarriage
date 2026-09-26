#ifndef CROWNLESS_JOURNAL_INTERNAL_H
#define CROWNLESS_JOURNAL_INTERNAL_H

#include "sim/cc_sim.h"
#include <sqlite3.h>

/* Shared record contract for journal writes and historical replay.

   Version 2 records were written by builds with deterministic arithmetic
   (no FMA contraction, cc_dmath.h), so every platform replays them to the
   same committed hashes, and replay checks every hash strictly.

   Version 1 records were written before that, mostly by the macOS arm64
   client, whose compiler fused a*b+c into FMA. Their committed hashes can
   disagree with today's arithmetic in the last unit of road geometry. A
   version 1 journal still replays its commands in order and checks each
   hash; after the first hash that does not match, it keeps replaying the
   player's commands without hash checks and stops quietly at the first
   command that no longer applies. The load reports this through the
   replay observer, and CcJournalResume starts a fresh version 2 epoch from
   the replayed state. */
#define CC_JOURNAL_RECORD_VERSION 2
#define CC_JOURNAL_LEGACY_ARITHMETIC_RECORD_VERSION 1
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
    bool *legacy_epoch, char *error, size_t error_capacity);

#endif
