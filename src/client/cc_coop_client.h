#ifndef CROWNLESS_COOP_CLIENT_H
#define CROWNLESS_COOP_CLIENT_H
#include "sim/cc_sim.h"

bool CcCoopClientActive(void);
bool CcCoopClientOwner(void);
bool CcCoopClientDelete(char *error, size_t capacity);
void CcCoopClientOpenLobby(void);
void CcCoopClientReturnToTitle(void);
void CcCoopClientReady(const char *error);
bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity);
#endif
