#ifndef CROWNLESS_SAVE_H
#define CROWNLESS_SAVE_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>

bool CcSaveWrite(const char *path, const CcSim *sim,
                 char *error, size_t error_capacity);
bool CcSaveRead(const char *path, CcSim *sim,
                char *error, size_t error_capacity);

#endif
