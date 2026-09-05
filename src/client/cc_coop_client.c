#include "client/cc_coop_client.h"
#include "persistence/cc_save.h"
#include "multiplayer/cc_coop_commands.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

EM_JS(int, CoopEnabled, (), { return Module.ccCoop && Module.ccCoop.enabled ? 1 : 0; });
EM_JS(int, CoopOwner, (), { return Module.ccCoop.owner() ? 1 : 0; });
EM_JS(void, CoopLobby, (), { Module.ccCoop.openLobby(); });
EM_JS(void, CoopTitle, (), { location.assign(location.pathname); });
EM_ASYNC_JS(int, CoopDelete, (char *error, int capacity), {
    try { await Module.ccCoop.deleteWorld(); return 1; }
    catch (failure) { stringToUTF8(failure.message, error, capacity); return 0; }
});
EM_JS(int, CoopPreview, (), { return Module.ccCoop && Module.ccCoop.preview ? 1 : 0; });
EM_JS(int, CoopHasSession, (), { return Module.ccCoop && Module.ccCoop.hasSession() ? 1 : 0; });
EM_JS(void, CoopCheckpoint, (const char *path), {
    Module.ccCoop.checkpoint(FS.readFile(UTF8ToString(path), {encoding:'utf8'}));
});
EM_JS(int, CoopAppearance, (), {
    const value = Module.ccCoop ? Module.ccCoop.avatar() : 0;
    document.body.dataset.avatar = String(value);
    return value;
});
EM_JS(void, CoopReady, (const char *error), { Module.ccCoop.ready(UTF8ToString(error)); });
EM_JS(void, CoopSynced, (const char *hash), { document.body.dataset.companyHash = UTF8ToString(hash); });
EM_ASYNC_JS(int, CoopConnect, (char *error, int capacity), {
    try { await Module.ccCoop.connect(); return 1; }
    catch (failure) { stringToUTF8(failure.message, error, capacity); return 0; }
});
EM_ASYNC_JS(int, CoopApply, (const char *action, const char *target, int good, int amount, char *error, int capacity), {
    try {
        const result = await Module.ccCoop.apply(UTF8ToString(action), UTF8ToString(target), good, amount);
        if (!result.accepted) stringToUTF8(result.message, error, capacity);
        return result.accepted ? 1 : 0;
    } catch (failure) { stringToUTF8(failure.message, error, capacity); return 0; }
});
EM_JS(unsigned char *, CoopTake, (int *length), {
    const coop = Module.ccCoop;
    if (!coop || !coop.enabled) return 0;
    coop.poll();
    const encoded = coop.take();
    if (!encoded) return 0;
    const raw = atob(encoded);
    if (raw.length > 8 * 1024 * 1024) return 0;
    const pointer = _malloc(raw.length);
    if (!pointer) return 0;
    for (let i = 0; i < raw.length; ++i) HEAPU8[pointer + i] = raw.charCodeAt(i);
    HEAP32[length >> 2] = raw.length;
    return pointer;
});

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

bool CcCoopClientActive(void) { return CoopEnabled() != 0; }
bool CcCoopClientOwner(void) { return CoopOwner() != 0; }
bool CcCoopClientDelete(char *error, size_t capacity) { return CoopDelete(error, (int)capacity) != 0; }
void CcCoopClientOpenLobby(void) { CoopLobby(); }
void CcCoopClientReturnToTitle(void) { CoopTitle(); }
bool CcCoopClientPreview(void) { return CoopPreview() != 0; }
uint32_t CcCoopClientAppearance(void) { return (uint32_t)CoopAppearance(); }
bool CcCoopClientHasSession(void) { return CoopHasSession() != 0; }
void CcCoopClientCheckpoint(const char *path) { CoopCheckpoint(path); }
void CcCoopClientReady(const char *error) { CoopReady(error); }

bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity)
{
    int length = 0;
    unsigned char *bytes = CoopTake(&length);
    if (bytes == NULL) return false;
    bool ok = CcSaveDecode(bytes, (size_t)length, sim, error, capacity);
    free(bytes);
    if (ok) {
        char hash[24];
        (void)snprintf(hash, sizeof(hash), "%016" PRIx64, CcSimHash(sim));
        CoopSynced(hash);
    }
    return ok;
}

bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity)
{
    return CoopConnect(error, (int)capacity) != 0 && CcCoopClientPoll(sim, error, capacity);
}

bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    char target[24];
    (void)snprintf(target, sizeof(target), "%" PRIu64, command->target_id);
    int amount = command->kind == CC_COMMAND_CHANGE_DUNGEON ? (int)command->dungeon_state : command->amount;
    bool applied = CoopApply(CcCoopActionName(command->kind), target, (int)command->good,
                             amount, error, (int)capacity) != 0;
    char sync_error[256];
    bool refreshed = CcCoopClientPoll(sim, sync_error, sizeof(sync_error));
    if (applied && !refreshed) {
        (void)snprintf(error, capacity, "The company action was saved. Reconnect to refresh the carriage.");
    }
    return applied && refreshed;
}
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity)
{
    bool applied = CoopApply("skip_watch", "0", 0, 0, error, (int)capacity) != 0;
    char sync_error[256];
    bool refreshed = CcCoopClientPoll(sim, sync_error, sizeof(sync_error));
    return applied && refreshed;
}

#else
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity)
{
    (void)sim; (void)error; (void)capacity; return false;
}
bool CcCoopClientActive(void) { return false; }
bool CcCoopClientOwner(void) { return false; }
bool CcCoopClientDelete(char *error, size_t capacity) { (void)error; (void)capacity; return false; }
void CcCoopClientOpenLobby(void) { OpenURL("https://crownless.ratimics.com/"); }
void CcCoopClientReturnToTitle(void) {}
bool CcCoopClientPreview(void) { return false; }
uint32_t CcCoopClientAppearance(void) { return 0U; }
bool CcCoopClientHasSession(void) { return false; }
void CcCoopClientCheckpoint(const char *path) { (void)path; }
void CcCoopClientReady(const char *error) { (void)error; }
bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity)
{
    (void)sim; (void)error; (void)capacity; return false;
}
bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    (void)sim; (void)command; (void)error; (void)capacity; return false;
}
bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity)
{
    (void)sim; (void)error; (void)capacity; return false;
}
#endif
