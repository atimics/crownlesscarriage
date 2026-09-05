#ifndef CROWNLESS_COOP_CLIENT_H
#define CROWNLESS_COOP_CLIENT_H
#include "sim/cc_sim.h"
#include "client/cc_crew.h"

bool CcCoopClientActive(void);
void CcCoopClientConfigure(const char *program, const char *campaign_path, const char *world);
void CcCoopClientWorldId(char *world, size_t capacity);
void CcCoopClientEnterWorld(const char *world);
void CcCoopClientShutdown(void);
bool CcCoopClientDead(void);
int32_t CcCoopClientPartyWipes(void);
void CcCoopClientLife(bool dead);
bool CcCoopClientOwner(void);
bool CcCoopClientDelete(char *error, size_t capacity);
void CcCoopClientOpenLobby(void);
void CcCoopClientReturnToTitle(void);
void CcCoopClientOpenCompany(void);
bool CcCoopClientSetAppearance(uint32_t choices, char *error, size_t capacity);
bool CcCoopClientPaused(void);
bool CcCoopClientTogglePause(char *error, size_t capacity);
uint32_t CcCoopClientAppearance(void);
bool CcCoopClientHasSession(void);
int32_t CcCoopClientSeat(void);
int32_t CcCoopClientExchange(int32_t scene, const float *pose, CcCrewMember *crew);
void CcCoopClientDrawn(const CcCrewMember *crew, int32_t count);
void CcCoopClientCheckpoint(const char *path);
void CcCoopCheckpointNow(void);
void CcCoopClientReady(const char *error);
bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity);
bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity);
#endif
