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

/* Creates a new campaign file and starts its append-only journal. */
CcJournal *CcJournalStart(const char *path, const CcSim *sim,
                          char *error, size_t error_capacity);
/* Explicitly starts a new campaign epoch in an existing Crownless save. */
CcJournal *CcJournalRestart(const char *path, const CcSim *sim,
                            char *error, size_t error_capacity);
/* Loads the last checkpoint, replays its durable suffix, and resumes writing. */
CcJournal *CcJournalResume(const char *path, CcSim *sim,
                           char *error, size_t error_capacity);
/* Replaces the disposable snapshot after first flushing pending runtime ticks. */
bool CcJournalCheckpoint(CcJournal *journal, CcSim *sim,
                         char *error, size_t error_capacity);
/* Mutations commit to SQLite before their candidate state becomes authoritative. */
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
/* On failure the journal remains open and `sim` is restored to its durable prefix. */
bool CcJournalClose(CcJournal **journal, CcSim *sim,
                    char *error, size_t error_capacity);
/* Closes a stale or unwanted writer without changing its campaign file. */
void CcJournalAbandon(CcJournal **journal);

#endif
