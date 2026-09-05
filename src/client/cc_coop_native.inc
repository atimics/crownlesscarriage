/* Native transport shares the host protocol and the in-game company menus. */
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sqlite3.h>

typedef enum NativeRequestKind { NATIVE_IDLE, NATIVE_STATE, NATIVE_POSE, NATIVE_SESSION } NativeRequestKind;
static struct {
    char program[1024], campaign[640], world[33], save[704], pending_path[768], session_path[768];
    char context[65], visit[33], member[17], error[256];
    long long revision, action_revision, sequence, session_sequence, session_saved;
    bool connected, owner, paused, dead, restored, closed, needs_enter;
    int32_t wipes, seat, scene, peer_count;
    uint32_t appearance;
    char *campaign_data, *checkpoint;
    CcCrewMember peers[CC_CREW_CAPACITY];
    double last_state, last_pose, last_session, peers_at;
    pthread_t worker;
    atomic_bool done;
    NativeRequestKind kind;
    char path[192], body[16000], request_context[65];
    char *response;
    int status, request_scene;
    bool ok;
} coop_native;

static double NativeNow(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static bool NativeWorld(const char *world)
{
    if (strlen(world) != 32) return false;
    for (int i = 0; i < 32; ++i)
        if (!((world[i] >= '0' && world[i] <= '9') || (world[i] >= 'a' && world[i] <= 'f'))) return false;
    return true;
}

static char *NativeRead(const char *path, size_t limit)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    char *text = malloc(limit + 1);
    size_t length = text != NULL ? fread(text, 1, limit, file) : 0;
    bool ok = text != NULL && !ferror(file) && feof(file);
    fclose(file);
    if (!ok) { free(text); return NULL; }
    text[length] = '\0';
    return text;
}

static bool NativeWrite(const char *path, const char *text)
{
    char temporary[800];
    (void)snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    size_t length = strlen(text);
    bool ok = write(fd, text, length) == (ssize_t)length && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) (void)unlink(temporary);
    return ok;
}

static bool NativeRequest(const char *path, const char *body, char **result, int *status, char *error, size_t capacity)
{
    char url[256];
    (void)snprintf(url, sizeof(url), "/api/worlds/%s/%s", coop_native.world, path);
    return CcCompanyRequestStatus(url, body, result, status, error, capacity);
}

static uint32_t NativeAppearance(const char *json, const char *path)
{
    const char *fields[] = {"skin", "hair", "style", "face", "coat"};
    uint32_t packed = 0;
    for (int i = 0; i < 5; ++i) {
        char key[96];
        (void)snprintf(key, sizeof(key), "%s.%s", path, fields[i]);
        long long value = CcCompanyJsonNumber(json, key);
        if (value >= 0 && value < (i == 3 ? 4 : 6)) packed |= (uint32_t)value << (i * 3);
    }
    return packed;
}

static void NativeAccept(const char *json)
{
    if (json == NULL) return;
    long long revision = CcCompanyJsonNumber(json, "$.revision");
    if (coop_native.connected && revision < coop_native.revision) return;
    char context[65];
    if (!CcCompanyJsonText(json, "$.session_context", context, sizeof(context))) return;
    if (strcmp(context, coop_native.context) != 0) {
        coop_native.peer_count = 0;
        free(coop_native.checkpoint);
        coop_native.checkpoint = NULL;
    }
    (void)snprintf(coop_native.context, sizeof(coop_native.context), "%s", context);
    int32_t wipes = (int32_t)CcCompanyJsonNumber(json, "$.party_wipes");
    if (wipes > coop_native.wipes) coop_native.dead = false;
    coop_native.wipes = wipes;
    coop_native.dead = coop_native.dead || CcCompanyJsonNumber(json, "$.dead") != 0;
    coop_native.owner = CcCompanyJsonNumber(json, "$.owner") != 0;
    coop_native.paused = CcCompanyJsonNumber(json, "$.paused") != 0;
    coop_native.action_revision = CcCompanyJsonNumber(json, "$.action_revision");
    coop_native.sequence = CcCompanyJsonNumber(json, "$.next_sequence");
    coop_native.appearance = NativeAppearance(json, "$.appearance");
    (void)CcCompanyJsonText(json, "$.member", coop_native.member, sizeof(coop_native.member));
    for (int i = 0; i < 8; ++i) {
        char path[48], member[17];
        (void)snprintf(path, sizeof(path), "$.crew[%d].id", i);
        if (CcCompanyJsonText(json, path, member, sizeof(member)) && strcmp(member, coop_native.member) == 0)
            coop_native.seat = i;
    }
    char visit[33];
    if (CcCompanyJsonText(json, "$.visit", visit, sizeof(visit))) {
        (void)snprintf(coop_native.visit, sizeof(coop_native.visit), "%s", visit);
        coop_native.needs_enter = false;
    }
    char *campaign = CcCompanyJsonValue(json, "$.campaign");
    if (campaign != NULL) {
        free(coop_native.campaign_data);
        coop_native.campaign_data = campaign;
    }
    coop_native.revision = revision;
    coop_native.connected = true;
}

static void NativePeers(const char *json)
{
    char *world = CcCompanyJsonValue(json, "$.world");
    NativeAccept(world);
    free(world);
    char context[65];
    if (!CcCompanyJsonText(json, "$.context", context, sizeof(context)) ||
        strcmp(context, coop_native.context) != 0 || coop_native.scene != coop_native.request_scene) return;
    coop_native.peer_count = 0;
    for (int i = 0; i < CC_CREW_CAPACITY; ++i) {
        char path[96];
        CcCrewMember peer = {0};
        (void)snprintf(path, sizeof(path), "$.peers[%d].id", i);
        if (!CcCompanyJsonText(json, path, peer.id, sizeof(peer.id))) break;
        (void)snprintf(path, sizeof(path), "$.peers[%d].name", i);
        (void)CcCompanyJsonText(json, path, peer.name, sizeof(peer.name));
        (void)snprintf(path, sizeof(path), "$.peers[%d].appearance", i);
        peer.appearance = NativeAppearance(json, path);
        (void)snprintf(path, sizeof(path), "$.peers[%d].pose", i);
        char *pose = CcCompanyJsonValue(json, path);
        sqlite3 *db = NULL;
        sqlite3_stmt *query = NULL;
        int count = 0;
        if (pose != NULL && sqlite3_open(":memory:", &db) == SQLITE_OK &&
            sqlite3_prepare_v2(db, "SELECT value FROM json_each(?1)", -1, &query, NULL) == SQLITE_OK) {
            sqlite3_bind_text(query, 1, pose, -1, SQLITE_STATIC);
            while (count < CC_CREW_POSE_FLOATS && sqlite3_step(query) == SQLITE_ROW) {
                double value = sqlite3_column_double(query, 0);
                if (!isfinite(value) || fabs(value) > 1000000.0) break;
                peer.pose[count++] = (float)value;
            }
        }
        sqlite3_finalize(query);
        sqlite3_close(db);
        free(pose);
        if (count == CC_CREW_POSE_FLOATS) coop_native.peers[coop_native.peer_count++] = peer;
    }
    coop_native.peers_at = NativeNow();
}

static void *NativeWorker(void *unused)
{
    (void)unused;
    coop_native.ok = NativeRequest(coop_native.path, coop_native.body[0] ? coop_native.body : NULL,
        &coop_native.response, &coop_native.status, coop_native.error, sizeof(coop_native.error));
    atomic_store(&coop_native.done, true);
    return NULL;
}

static void NativeFinish(bool wait)
{
    if (coop_native.kind == NATIVE_IDLE || (!wait && !atomic_load(&coop_native.done))) return;
    (void)pthread_join(coop_native.worker, NULL);
    if (coop_native.ok) {
        if (coop_native.kind == NATIVE_STATE) NativeAccept(coop_native.response);
        if (coop_native.kind == NATIVE_POSE) NativePeers(coop_native.response);
        if (coop_native.kind == NATIVE_SESSION) {
            long long saved = CcCompanyJsonNumber(coop_native.response, "$.sequence");
            if (saved > coop_native.session_saved) coop_native.session_saved = saved;
        }
    } else if (coop_native.kind == NATIVE_POSE) {
        coop_native.peer_count = 0;
        if (coop_native.status == 428) coop_native.needs_enter = true;
        if (coop_native.status == 409) coop_native.last_pose = NativeNow() + 0.9;
    }
    free(coop_native.response);
    coop_native.response = NULL;
    coop_native.kind = NATIVE_IDLE;
}

static void NativeStart(NativeRequestKind kind, const char *path, const char *body)
{
    if (coop_native.kind != NATIVE_IDLE || coop_native.closed) return;
    (void)snprintf(coop_native.path, sizeof(coop_native.path), "%s", path);
    (void)snprintf(coop_native.body, sizeof(coop_native.body), "%s", body != NULL ? body : "");
    coop_native.kind = kind;
    atomic_store(&coop_native.done, false);
    if (pthread_create(&coop_native.worker, NULL, NativeWorker, NULL) != 0) coop_native.kind = NATIVE_IDLE;
}

static bool NativeRefresh(char *error, size_t capacity)
{
    NativeFinish(true);
    char *json = NULL;
    int status;
    bool ok = NativeRequest("state?campaign=1", NULL, &json, &status, error, capacity);
    if (ok) NativeAccept(json);
    free(json);
    return ok;
}

void CcCoopClientConfigure(const char *program, const char *campaign_path, const char *world)
{
    (void)snprintf(coop_native.program, sizeof(coop_native.program), "%s", program);
    (void)snprintf(coop_native.campaign, sizeof(coop_native.campaign), "%s", campaign_path);
    if (NativeWorld(world)) (void)snprintf(coop_native.world, sizeof(coop_native.world), "%s", world);
    (void)snprintf(coop_native.save, sizeof(coop_native.save), "%.560s.shared-%.32s", campaign_path, world);
    (void)snprintf(coop_native.pending_path, sizeof(coop_native.pending_path), "%s.pending", coop_native.save);
    (void)snprintf(coop_native.session_path, sizeof(coop_native.session_path), "%s.checkpoint", coop_native.save);
    coop_native.scene = -1;
    coop_native.last_state = coop_native.last_pose = coop_native.last_session = -100.0;
}

bool CcCoopClientActive(void) { return coop_native.world[0] != '\0'; }
void CcCoopClientWorldId(char *world, size_t capacity) { (void)snprintf(world, capacity, "%s", coop_native.world); }
bool CcCoopClientDead(void) { return coop_native.dead; }
int32_t CcCoopClientPartyWipes(void) { return coop_native.wipes; }
void CcCoopClientLife(bool dead) { coop_native.dead = coop_native.dead || dead; }
bool CcCoopClientOwner(void) { return coop_native.owner; }
bool CcCoopClientPaused(void) { return coop_native.paused; }
uint32_t CcCoopClientAppearance(void) { return coop_native.appearance; }
bool CcCoopClientHasSession(void) { return coop_native.restored; }
int32_t CcCoopClientSeat(void) { return coop_native.seat; }
void CcCoopClientReady(const char *error) { if (error[0]) TraceLog(LOG_WARNING, "%s", error); }
void CcCoopClientDrawn(const CcCrewMember *crew, int32_t count) { (void)crew; (void)count; }
void CcCoopClientOpenLobby(void) {}
void CcCoopClientOpenCompany(void) {}

static void NativeRestoreSession(const char *json)
{
    char *host = CcCompanyJsonValue(json, "$.session");
    char *local = NativeRead(coop_native.session_path, 16000);
    const char *candidates[] = {host, local};
    coop_native.session_saved = host != NULL ? CcCompanyJsonNumber(host, "$.sequence") : 0;
    for (int i = 0; i < 2; ++i) {
        if (candidates[i] == NULL) continue;
        long long sequence = CcCompanyJsonNumber(candidates[i], "$.sequence");
        if (sequence > coop_native.session_sequence) coop_native.session_sequence = sequence;
        char context[65];
        if (!CcCompanyJsonText(candidates[i], "$.context", context, sizeof(context)) ||
            strcmp(context, coop_native.context) != 0 ||
            (coop_native.checkpoint != NULL && sequence <= CcCompanyJsonNumber(coop_native.checkpoint, "$.sequence"))) continue;
        char *session = CcCompanyJsonValue(candidates[i], "$.session");
        if (session != NULL && strlen(session) <= 6000 && strncmp(session, "CROWNLESS_SESSION 7\n", 20) == 0) {
            char path[768];
            (void)snprintf(path, sizeof(path), "%s.session", coop_native.save);
            if (NativeWrite(path, session)) {
                free(coop_native.checkpoint);
                size_t size = strlen(candidates[i]) + 1;
                coop_native.checkpoint = malloc(size);
                if (coop_native.checkpoint != NULL) memcpy(coop_native.checkpoint, candidates[i], size);
                coop_native.restored = true;
            }
        }
        free(session);
    }
    free(host); free(local);
    if (!coop_native.restored) {
        char path[768];
        (void)snprintf(path, sizeof(path), "%s.session", coop_native.save);
        (void)unlink(path);
    }
}

bool CcCoopClientConnect(CcSim *sim, char *error, size_t capacity)
{
    char *json = NULL;
    int status;
    bool ok = NativeRequest("state?campaign=1&enter=1", NULL, &json, &status, error, capacity);
    if (ok && CcCompanyJsonNumber(json, "$.catching_up") != 0) {
        (void)snprintf(error, capacity, "The world is catching up. Enter again shortly.");
        ok = false;
    }
    if (ok) { NativeAccept(json); NativeRestoreSession(json); }
    free(json);
    return ok && CcCoopClientPoll(sim, error, capacity);
}

bool CcCoopClientPoll(CcSim *sim, char *error, size_t capacity)
{
    NativeFinish(false);
    double now = NativeNow();
    if (coop_native.connected && coop_native.kind == NATIVE_IDLE && !coop_native.closed) {
        if (now - coop_native.last_state >= 0.75) {
            char path[128];
            (void)snprintf(path, sizeof(path), "state?campaign=1&after=%lld", coop_native.revision);
            coop_native.last_state = now;
            NativeStart(NATIVE_STATE, coop_native.needs_enter ? "state?campaign=1&enter=1" : path, NULL);
        } else if (coop_native.checkpoint != NULL && coop_native.session_sequence > coop_native.session_saved &&
                   now - coop_native.last_session >= 1.0) {
            coop_native.last_session = now;
            NativeStart(NATIVE_SESSION, "session", coop_native.checkpoint);
        }
    }
    if (coop_native.campaign_data == NULL) return false;
    int length = 0;
    size_t encoded_length = strlen(coop_native.campaign_data);
    bool valid = encoded_length >= 4 && encoded_length % 4 == 0 && encoded_length <= 12U * 1024U * 1024U;
    for (size_t i = 0; valid && i < encoded_length; ++i) {
        char c = coop_native.campaign_data[i];
        valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' ||
            (c == '=' && i >= encoded_length - 2);
    }
    unsigned char *bytes = valid ? DecodeDataBase64(coop_native.campaign_data, &length) : NULL;
    free(coop_native.campaign_data);
    coop_native.campaign_data = NULL;
    bool ok = bytes != NULL && length > 0 && length <= 8 * 1024 * 1024 &&
        CcSaveDecode(bytes, (size_t)length, sim, error, capacity);
    MemFree(bytes);
    return ok;
}

static bool NativeApply(CcSim *sim, const char *action, const char *target, int good, int amount, char *error, size_t capacity)
{
    NativeFinish(true);
    char body[1024];
    (void)snprintf(body, sizeof(body), "{\"protocol\":1,\"sequence\":%lld,\"action_revision\":%lld,\"action\":\"%s\",\"target\":\"%s\",\"good\":%d,\"amount\":%d,\"campaign\":true}",
        coop_native.sequence, coop_native.action_revision, action, target, good, amount);
    char *pending = NativeRead(coop_native.pending_path, sizeof(body));
    bool other = false;
    if (pending != NULL) {
        char prior_action[80], prior_target[32];
        other = !CcCompanyJsonText(pending, "$.action", prior_action, sizeof(prior_action)) ||
            !CcCompanyJsonText(pending, "$.target", prior_target, sizeof(prior_target)) ||
            strcmp(prior_action, action) != 0 || strcmp(prior_target, target) != 0 ||
            CcCompanyJsonNumber(pending, "$.good") != good || CcCompanyJsonNumber(pending, "$.amount") != amount;
    } else if (!NativeWrite(coop_native.pending_path, body)) {
        (void)snprintf(error, capacity, "Check access to your company save folder.");
        return false;
    }
    char *json = NULL;
    int status;
    bool ok = NativeRequest("command", pending != NULL ? pending : body, &json, &status, error, capacity);
    free(pending);
    if (ok || (status >= 400 && status < 500)) (void)unlink(coop_native.pending_path);
    bool accepted = ok && CcCompanyJsonNumber(json, "$.accepted") != 0;
    if (ok) {
        char *world = CcCompanyJsonValue(json, "$.world");
        NativeAccept(world); free(world);
        if (!accepted) (void)CcCompanyJsonText(json, "$.message", error, capacity);
    } else if (status >= 400 && status < 500) {
        char refresh_error[256];
        (void)NativeRefresh(refresh_error, sizeof(refresh_error));
    }
    free(json);
    char sync_error[256];
    bool synced = CcCoopClientPoll(sim, sync_error, sizeof(sync_error));
    if (ok && other) (void)snprintf(error, capacity, "Earlier company action recovered. Choose your next action again.");
    if (accepted && !synced) (void)snprintf(error, capacity, "The action was saved. Reconnect to refresh the carriage.");
    return accepted && synced && !other;
}

bool CcCoopClientApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    char target[24];
    (void)snprintf(target, sizeof(target), "%" PRIu64, command->target_id);
    int amount = command->kind == CC_COMMAND_CHANGE_DUNGEON ? (int)command->dungeon_state : command->amount;
    return NativeApply(sim, CcCoopActionName(command->kind), target, (int)command->good, amount, error, capacity);
}
bool CcCoopClientSkip(CcSim *sim, char *error, size_t capacity)
{ return NativeApply(sim, "skip_watch", "0", 0, 0, error, capacity); }

int32_t CcCoopClientExchange(int32_t scene, const float *pose, CcCrewMember *crew)
{
    NativeFinish(false);
    if (scene != coop_native.scene) { coop_native.scene = scene; coop_native.peer_count = 0; }
    double now = NativeNow();
    if (coop_native.connected && coop_native.visit[0] && coop_native.kind == NATIVE_IDLE &&
        !coop_native.closed && now - coop_native.last_pose >= 0.1) {
        char body[4096];
        size_t used = (size_t)snprintf(body, sizeof(body), "{\"visit\":\"%s\",\"context\":\"%s\",\"scene\":%d,\"dead\":%s,\"pose\":[",
            coop_native.visit, coop_native.context, scene, coop_native.dead ? "true" : "false");
        for (int i = 0; i < CC_CREW_POSE_FLOATS; ++i) {
            if (!isfinite(pose[i]) || fabsf(pose[i]) > 1000000.0f) return 0;
            int added = snprintf(body + used, sizeof(body) - used, "%s%.3f", i ? "," : "", (double)pose[i]);
            if (added < 0 || (size_t)added >= sizeof(body) - used - 3) return 0;
            used += (size_t)added;
        }
        (void)snprintf(body + used, sizeof(body) - used, "]}");
        coop_native.request_scene = scene;
        coop_native.last_pose = now;
        NativeStart(NATIVE_POSE, "pose", body);
    }
    if (now - coop_native.peers_at > 3.0) coop_native.peer_count = 0;
    memcpy(crew, coop_native.peers, (size_t)coop_native.peer_count * sizeof(*crew));
    return coop_native.peer_count;
}

void CcCoopClientCheckpoint(const char *path)
{
    if (!coop_native.connected || coop_native.closed) return;
    char *session = NativeRead(path, 6001);
    if (session == NULL) return;
    char *old = CcCompanyJsonValue(coop_native.checkpoint, "$.session");
    bool same = old != NULL && strcmp(session, old) == 0;
    free(old);
    if (same) { free(session); return; }
    char *quoted = CcCompanyJsonQuote(session);
    free(session);
    if (quoted == NULL) return;
    char body[16000];
    (void)snprintf(body, sizeof(body), "{\"sequence\":%lld,\"context\":\"%s\",\"session\":%s}",
        coop_native.session_sequence + 1, coop_native.context, quoted);
    free(quoted);
    if (!NativeWrite(coop_native.session_path, body)) return;
    ++coop_native.session_sequence;
    free(coop_native.checkpoint);
    coop_native.checkpoint = NativeRead(coop_native.session_path, sizeof(body));
}

bool CcCoopClientSetAppearance(uint32_t choices, char *error, size_t capacity)
{
    NativeFinish(true);
    char body[192], *json = NULL;
    int status;
    (void)snprintf(body, sizeof(body), "{\"appearance\":{\"skin\":%u,\"hair\":%u,\"style\":%u,\"face\":%u,\"coat\":%u}}",
        choices & 7U, (choices >> 3) & 7U, (choices >> 6) & 7U, (choices >> 9) & 7U, (choices >> 12) & 7U);
    bool ok = NativeRequest("appearance", body, &json, &status, error, capacity);
    if (ok) NativeAccept(json);
    free(json);
    return ok;
}

bool CcCoopClientTogglePause(char *error, size_t capacity)
{
    NativeFinish(true);
    char *json = NULL;
    int status;
    bool ok = NativeRequest("host", coop_native.paused ? "{\"action\":\"resume\"}" : "{\"action\":\"pause\"}",
        &json, &status, error, capacity);
    free(json);
    return ok && NativeRefresh(error, capacity);
}

bool CcCoopClientDelete(char *error, size_t capacity)
{
    NativeFinish(true);
    if (access(coop_native.pending_path, F_OK) == 0) {
        (void)snprintf(error, capacity, "Finish the pending company action first."); return false;
    }
    char *json = NULL;
    int status;
    bool ok = NativeRequest("host", "{\"action\":\"delete\"}", &json, &status, error, capacity);
    if (ok) coop_native.closed = true;
    free(json);
    return ok;
}

void CcCoopClientShutdown(void)
{
    if (!CcCoopClientActive()) return;
    CcCoopCheckpointNow();
    NativeFinish(true);
    char error[256], *json = NULL;
    int status;
    if (!coop_native.closed && coop_native.checkpoint != NULL && coop_native.session_sequence > coop_native.session_saved) {
        (void)NativeRequest("session", coop_native.checkpoint, &json, &status, error, sizeof(error));
        free(json); json = NULL;
    }
    if (!coop_native.closed && coop_native.visit[0]) {
        char body[256];
        (void)snprintf(body, sizeof(body), "{\"visit\":\"%s\",\"context\":\"%s\",\"scene\":%d,\"pose\":null}",
            coop_native.visit, coop_native.context, coop_native.scene < 0 ? 0 : coop_native.scene);
        (void)NativeRequest("pose", body, &json, &status, error, sizeof(error));
        free(json);
    }
    coop_native.closed = true;
    free(coop_native.checkpoint); coop_native.checkpoint = NULL;
    free(coop_native.campaign_data); coop_native.campaign_data = NULL;
}

void CcCoopClientEnterWorld(const char *world)
{
    if (!NativeWorld(world)) return;
    CcCoopClientShutdown();
    char *args[] = {coop_native.program, "--campaign", coop_native.campaign, "--shared-world", (char *)world, NULL};
    (void)execv(coop_native.program, args);
    TraceLog(LOG_WARNING, "Reopen the game to enter this world.");
}

void CcCoopClientReturnToTitle(void)
{
    CcCoopClientShutdown();
    char *args[] = {coop_native.program, "--campaign", coop_native.campaign, NULL};
    (void)execv(coop_native.program, args);
    TraceLog(LOG_WARNING, "Reopen the game to return to the title.");
}
