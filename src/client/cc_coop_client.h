#ifndef CROWNLESS_COOP_CLIENT_H
#define CROWNLESS_COOP_CLIENT_H
#include "sim/cc_sim.h"

bool CcCoopClientActive(void);
bool CcCoopClientPreview(void);
uint32_t CcCoopClientAppearance(void);
bool CcCoopClientHasSession(void);
void CcCoopClientCheckpoint(const char *path);
void CcCoopCheckpointNow(void);
void CcCoopClientReady(const char *error);
bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity);
#endif
