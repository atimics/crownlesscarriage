#ifndef CROWNLESS_SAVE_H
#define CROWNLESS_SAVE_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct CcJournal CcJournal;

bool CcSaveWrite(const char *path, const CcSim *sim,
                 char *error, size_t error_capacity);
bool CcSaveRead(const char *path, CcSim *sim,
                char *error, size_t error_capacity);
bool CcSaveRepairHash(const char *path,
                      char *error, size_t error_capacity);
bool CcSaveEncode(const CcSim *sim, unsigned char **bytes, size_t *length,
                  char *error, size_t error_capacity);
bool CcSaveDecode(const unsigned char *bytes, size_t length, CcSim *sim,
                  char *error, size_t error_capacity);
bool CcSaveDecodeRepair(const unsigned char *bytes, size_t length, CcSim *sim,
                        char *error, size_t error_capacity);
void CcSaveFreeBuffer(void *bytes);


CcJournal *CcJournalStart(const char *path, const CcSim *sim,
                          char *error, size_t error_capacity);

CcJournal *CcJournalRestart(const char *path, const CcSim *sim,
                            char *error, size_t error_capacity);

CcJournal *CcJournalResume(const char *path, CcSim *sim,
                           char *error, size_t error_capacity);

bool CcJournalCheckpoint(CcJournal *journal, CcSim *sim,
                         char *error, size_t error_capacity);

bool CcJournalApply(CcJournal *journal, CcSim *sim,
                    const CcCommand *command,
                    char *error, size_t error_capacity);
bool CcJournalAdvanceDays(CcJournal *journal, CcSim *sim, int32_t days,
                          char *error, size_t error_capacity);
bool CcJournalAdvanceRuntimeTicks(CcJournal *journal, CcSim *sim,
                                  int32_t ticks,
                                  char *error, size_t error_capacity);
bool CcJournalFlush(CcJournal *journal, CcSim *sim,
                    char *error, size_t error_capacity);

bool CcJournalClose(CcJournal **journal, CcSim *sim,
                    char *error, size_t error_capacity);

void CcJournalAbandon(CcJournal **journal);

/* Hook for journal replay on load. When set, it is called once for each
   replayed record, after the record is applied and before its committed
   post-state hash is checked. Tools use it to print a hash trace that can
   be compared across platforms, and tests use it to see when a legacy
   journal (written before deterministic arithmetic) no longer matched its
   committed hashes. It is process-wide state; pass NULL to clear it. */
typedef struct CcJournalReplayStep {
    uint64_t ordinal;
    int32_t operation_kind;
    int32_t command_kind;
    int32_t step_count;
    uint64_t pre_state_hash;
    uint64_t committed_post_state_hash;
    bool applied;
    /* The record was written before deterministic arithmetic. */
    bool legacy_arithmetic;
    /* This record and all before it reproduced their committed hashes. */
    bool verified;
} CcJournalReplayStep;
typedef void (*CcJournalReplayObserver)(void *context,
                                        const CcJournalReplayStep *step,
                                        const CcSim *sim);
void CcJournalSetReplayObserver(CcJournalReplayObserver observer,
                                void *context);

#endif
