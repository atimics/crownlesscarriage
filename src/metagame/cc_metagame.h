#ifndef CROWNLESS_METAGAME_H
#define CROWNLESS_METAGAME_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CcMetagame {
    CcSim sim;
    bool quit_requested;
} CcMetagame;

void CcMetagameInit(CcMetagame *metagame, uint32_t seed);
void CcMetagameIntro(const CcMetagame *metagame,
                     char *output, size_t output_capacity);
bool CcMetagameExecute(CcMetagame *metagame, const char *line,
                       char *output, size_t output_capacity);

#endif
