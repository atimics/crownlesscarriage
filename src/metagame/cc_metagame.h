#ifndef CROWNLESS_METAGAME_H
#define CROWNLESS_METAGAME_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CcJournal CcJournal;

typedef struct CcMetagame {
    CcSim sim;
    bool quit_requested;
    CcJournal *journal;
} CcMetagame;

void CcMetagameInit(CcMetagame *metagame, uint32_t seed);
bool CcMetagameStartJournal(CcMetagame *metagame, const char *path,
                            char *error, size_t error_capacity);
bool CcMetagameResumeJournal(CcMetagame *metagame, const char *path,
                             char *error, size_t error_capacity);
bool CcMetagameCloseJournal(CcMetagame *metagame,
                            char *error, size_t error_capacity);
void CcMetagameIntro(const CcMetagame *metagame,
                     char *output, size_t output_capacity);
bool CcMetagameExecute(CcMetagame *metagame, const char *line,
                       char *output, size_t output_capacity);
void CcMetagameAgentObserve(const CcMetagame *metagame,
                            char *output, size_t output_capacity);
bool CcMetagameAgentExecute(CcMetagame *metagame, const char *line,
                            char *output, size_t output_capacity);
bool CcMetagameAgentCounterfactual(const CcMetagame *metagame,
                                   char *output, size_t output_capacity,
                                   uint64_t *control_hash);

#endif
