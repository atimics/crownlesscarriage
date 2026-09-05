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
bool CcSaveEncode(const CcSim *sim, unsigned char **bytes, size_t *length,
                  char *error, size_t error_capacity);
bool CcSaveDecode(const unsigned char *bytes, size_t length, CcSim *sim,
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

#endif
