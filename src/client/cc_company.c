#include "client/cc_company.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char company_identity_path[768];
static char pending_create[33];

char *CcCompanyJsonValue(const char *json, const char *path)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *query = NULL;
    char *result = NULL;
    if (json != NULL && sqlite3_open(":memory:", &db) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "SELECT json_extract(?1,?2)", -1, &query, NULL) == SQLITE_OK) {
        sqlite3_bind_text(query, 1, json, -1, SQLITE_STATIC);
        sqlite3_bind_text(query, 2, path, -1, SQLITE_STATIC);
        if (sqlite3_step(query) == SQLITE_ROW && sqlite3_column_type(query, 0) != SQLITE_NULL) {
            const char *value = (const char *)sqlite3_column_text(query, 0);
            if (value != NULL) {
                size_t length = strlen(value) + 1;
                result = malloc(length);
                if (result != NULL) memcpy(result, value, length);
            }
        }
    }
    sqlite3_finalize(query);
    sqlite3_close(db);
    return result;
}

bool CcCompanyJsonText(const char *json, const char *path, char *text, size_t capacity)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *query = NULL;
    bool ok = false;
    if (capacity == 0) return false;
    text[0] = '\0';
    if (sqlite3_open(":memory:", &db) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "SELECT json_extract(?1,?2)", -1, &query, NULL) == SQLITE_OK) {
        sqlite3_bind_text(query, 1, json, -1, SQLITE_STATIC);
        sqlite3_bind_text(query, 2, path, -1, SQLITE_STATIC);
        if (sqlite3_step(query) == SQLITE_ROW && sqlite3_column_type(query, 0) != SQLITE_NULL) {
            const char *value = (const char *)sqlite3_column_text(query, 0);
            if (value != NULL && strlen(value) < capacity) {
                (void)snprintf(text, capacity, "%s", value);
                ok = true;
            }
        }
    }
    sqlite3_finalize(query);
    sqlite3_close(db);
    return ok;
}

long long CcCompanyJsonNumber(const char *json, const char *path)
{
    char text[32];
    return CcCompanyJsonText(json, path, text, sizeof(text)) ? strtoll(text, NULL, 10) : 0;
}

char *CcCompanyJsonQuote(const char *text)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *query = NULL;
    char *result = NULL;
    if (sqlite3_open(":memory:", &db) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "SELECT json_quote(?1)", -1, &query, NULL) == SQLITE_OK) {
        sqlite3_bind_text(query, 1, text, -1, SQLITE_STATIC);
        if (sqlite3_step(query) == SQLITE_ROW) {
            const char *quoted = (const char *)sqlite3_column_text(query, 0);
            size_t length = strlen(quoted) + 1;
            result = malloc(length);
            if (result != NULL) memcpy(result, quoted, length);
        }
    }
    sqlite3_finalize(query);
    sqlite3_close(db);
    return result;
}

static bool Hex(const char *text, size_t length)
{
    if (strlen(text) != length) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

void CcCompanyConfigure(const char *identity_path)
{
    (void)snprintf(company_identity_path, sizeof(company_identity_path), "%s", identity_path);
}

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
EM_JS(int, CompanyRandom, (char *text, int bytes), {
    const value = Array.from(crypto.getRandomValues(new Uint8Array(bytes)), n => n.toString(16).padStart(2, '0')).join('');
    stringToUTF8(value, text, bytes * 2 + 1);
    return 1;
});
EM_ASYNC_JS(int, CompanyRequest, (const char *path, const char *body, char **result, int *status, char *error, int capacity), {
    try {
        let token = localStorage.getItem('cc-coop-token');
        if (!/^[a-f0-9]{64}$/.test(token || '')) {
            token = Array.from(crypto.getRandomValues(new Uint8Array(32)), n => n.toString(16).padStart(2, '0')).join('');
            localStorage.setItem('cc-coop-token', token);
        }
        const response = await fetch(UTF8ToString(path), {method: body ? 'POST' : 'GET',
            headers: {Authorization: 'Bearer ' + token, 'Content-Type': 'application/json'},
            body: body ? UTF8ToString(body) : undefined, signal: AbortSignal.timeout(12000)});
        const data = await response.text();
        HEAP32[status >> 2] = response.status;
        const parsed = JSON.parse(data);
        if (!response.ok) throw new Error(parsed.error || 'Reconnect to the host.');
        if (data.length > 12 * 1024 * 1024) throw new Error('The host response is too large.');
        const size = lengthBytesUTF8(data) + 1, pointer = _malloc(size);
        if (!pointer) throw new Error('Free some memory and try again.');
        stringToUTF8(data, pointer, size);
        HEAPU32[result >> 2] = pointer;
        return 1;
    } catch (failure) { stringToUTF8(failure.message, error, capacity); return 0; }
});
#pragma clang diagnostic pop
bool CcCompanyRequestStatus(const char *path, const char *body, char **result, int *status, char *error, size_t capacity)
{
    *result = NULL;
    *status = 0;
    return CompanyRequest(path, body, result, status, error, (int)capacity) != 0;
}
#else
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static int CompanyRandom(char *text, int bytes)
{
    unsigned char random[32];
    if (bytes < 1 || bytes > 32) return 0;
    FILE *source = fopen("/dev/urandom", "rb");
    if (source == NULL) return 0;
    bool ok = fread(random, 1, (size_t)bytes, source) == (size_t)bytes;
    fclose(source);
    if (!ok) return 0;
    for (int i = 0; i < bytes; ++i) (void)snprintf(text + i * 2, 3, "%02x", random[i]);
    return 1;
}

typedef struct CompanyResponse { char *data; size_t length; } CompanyResponse;
static size_t CompanyWrite(char *data, size_t size, size_t count, void *user)
{
    CompanyResponse *response = user;
    if (size != 0 && count > (12U * 1024U * 1024U - response->length) / size) return 0;
    size_t bytes = size * count;
    char *next = realloc(response->data, response->length + bytes + 1);
    if (next == NULL) return 0;
    response->data = next;
    memcpy(next + response->length, data, bytes);
    response->length += bytes;
    next[response->length] = '\0';
    return bytes;
}

static bool CompanyIdentity(char *token)
{
    FILE *file = fopen(company_identity_path, "rb");
    if (file != NULL) {
        size_t length = fread(token, 1, 65, file);
        fclose(file);
        token[length < 65 ? length : 64] = '\0';
        return length == 64 && Hex(token, 64);
    }
    if (errno != ENOENT || !CompanyRandom(token, 32)) return false;
    int fd = open(company_identity_path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    bool ok = write(fd, token, 64) == 64;
    if (close(fd) != 0) ok = false;
    return ok;
}

bool CcCompanyRequestStatus(const char *path, const char *body, char **result, int *http_status, char *error, size_t capacity)
{
    char token[66], authorization[96], url[512];
    *result = NULL;
    *http_status = 0;
    if (!CompanyIdentity(token)) {
        (void)snprintf(error, capacity, "Check access to your saved company identity.");
        return false;
    }
    const char *origin = getenv("CC_COOP_ORIGIN");
    if (origin == NULL) origin = "https://crownless.ratimics.com";
    (void)snprintf(url, sizeof(url), "%s%s", origin, path);
    (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", token);
    CURL *curl = curl_easy_init();
    if (curl == NULL) return false;
    struct curl_slist *headers = curl_slist_append(NULL, authorization);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    CompanyResponse response = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Crownless-Carriage/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CompanyWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if (body != NULL) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    CURLcode status = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    *http_status = (int)code;
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    bool ok = status == CURLE_OK && code >= 200 && code < 300 && response.data != NULL;
    if (ok) *result = response.data;
    else {
        if (response.data == NULL || !CcCompanyJsonText(response.data, "$.error", error, capacity))
            (void)snprintf(error, capacity, "Reconnect to the host and try again.");
        free(response.data);
    }
    return ok;
}
#endif

bool CcCompanyRequest(const char *path, const char *body, char **result, char *error, size_t capacity)
{
    int status;
    return CcCompanyRequestStatus(path, body, result, &status, error, capacity);
}

static bool CompanyRead(CcCompany *company, const char *json)
{
    CcCompany next = {0};
    if (!CcCompanyJsonText(json, "$.id", next.id, sizeof(next.id)) || !Hex(next.id, 32)) return false;
    (void)CcCompanyJsonText(json, "$.name", next.name, sizeof(next.name));
    next.owner = CcCompanyJsonNumber(json, "$.owner") != 0;
    next.paused = CcCompanyJsonNumber(json, "$.paused") != 0;
    char member[33];
    (void)CcCompanyJsonText(json, "$.member", member, sizeof(member));
    for (int i = 0; i < 8; ++i) {
        char path[64];
        (void)snprintf(path, sizeof(path), "$.crew[%d].id", i);
        if (!CcCompanyJsonText(json, path, next.crew[i].id, sizeof(next.crew[i].id))) break;
        (void)snprintf(path, sizeof(path), "$.crew[%d].name", i);
        (void)CcCompanyJsonText(json, path, next.crew[i].name, sizeof(next.crew[i].name));
        (void)snprintf(path, sizeof(path), "$.crew[%d].online", i);
        next.crew[i].online = CcCompanyJsonNumber(json, path) != 0;
        next.crew[i].self = strcmp(next.crew[i].id, member) == 0;
        ++next.crew_count;
    }
    *company = next;
    return true;
}

bool CcCompanyList(CcCompany *company, char *error, size_t capacity)
{
    char *json;
    if (!CcCompanyRequest("/api/worlds", NULL, &json, error, capacity)) return false;
    CcCompany next = {0};
    for (int i = 0; i < 16; ++i) {
        char path[64];
        (void)snprintf(path, sizeof(path), "$.worlds[%d].id", i);
        if (!CcCompanyJsonText(json, path, next.worlds[i].id, sizeof(next.worlds[i].id))) break;
        (void)snprintf(path, sizeof(path), "$.worlds[%d].name", i);
        (void)CcCompanyJsonText(json, path, next.worlds[i].name, sizeof(next.worlds[i].name));
        ++next.world_count;
    }
    free(json);
    *company = next;
    return true;
}

bool CcCompanyLoad(CcCompany *company, const char *world, char *error, size_t capacity)
{
    if (!Hex(world, 32)) return false;
    char path[96], *json;
    (void)snprintf(path, sizeof(path), "/api/worlds/%s/state", world);
    if (!CcCompanyRequest(path, NULL, &json, error, capacity)) return false;
    bool ok = CompanyRead(company, json);
    free(json);
    return ok;
}

static bool CompanySubmit(CcCompany *company, const char *path, const char *body, char *error, size_t capacity)
{
    char *json;
    if (!CcCompanyRequest(path, body, &json, error, capacity)) return false;
    bool ok = CompanyRead(company, json);
    free(json);
    return ok;
}

bool CcCompanyCreate(CcCompany *company, const char *player, const char *name, const char *pass, char *error, size_t capacity)
{
    if (!Hex(pass, 64)) { (void)snprintf(error, capacity, "Enter the world pass from this host."); return false; }
    if (pending_create[0] == '\0' && !CompanyRandom(pending_create, 16)) return false;
    char *person = CcCompanyJsonQuote(player), *title = CcCompanyJsonQuote(name);
    if (person == NULL || title == NULL) { free(person); free(title); return false; }
    char body[768];
    (void)snprintf(body, sizeof(body), "{\"id\":\"%s\",\"player\":%s,\"name\":%s,\"world_pass\":\"%s\"}", pending_create, person, title, pass);
    free(person); free(title);
    bool ok = CompanySubmit(company, "/api/worlds", body, error, capacity);
    if (ok) pending_create[0] = '\0';
    return ok;
}

bool CcCompanyJoin(CcCompany *company, const char *player, const char *invitation, char *error, size_t capacity)
{
    const char *start = strstr(invitation, "#join=");
    start = start != NULL ? start + 6 : invitation;
    char world[33];
    if (strlen(start) != 97 || start[32] != '.') goto invalid;
    memcpy(world, start, 32); world[32] = '\0';
    if (!Hex(world, 32) || !Hex(start + 33, 64)) goto invalid;
    char path[96], body[512];
    char *person = CcCompanyJsonQuote(player);
    if (person == NULL) return false;
    (void)snprintf(path, sizeof(path), "/api/worlds/%s/join", world);
    (void)snprintf(body, sizeof(body), "{\"player\":%s,\"invite\":\"%s\"}", person, start + 33);
    free(person);
    return CompanySubmit(company, path, body, error, capacity);
invalid:
    (void)snprintf(error, capacity, "Paste the complete invitation from your host.");
    return false;
}

bool CcCompanyInvite(CcCompany *company, bool rotate, char *error, size_t capacity)
{
    char path[96], invitation[65], *json;
    (void)snprintf(path, sizeof(path), "/api/worlds/%s/invite", company->id);
    if (!CcCompanyRequest(path, rotate ? "{}" : NULL, &json, error, capacity)) return false;
    bool ok = CcCompanyJsonText(json, "$.invite", invitation, sizeof(invitation)) && Hex(invitation, 64);
    if (ok) (void)snprintf(company->invitation, sizeof(company->invitation), "%s.%s", company->id, invitation);
    free(json);
    return ok;
}

bool CcCompanyHost(CcCompany *company, const char *action, const char *member, char *error, size_t capacity)
{
    char path[96], body[192], *json;
    if (strcmp(action, "pause") != 0 && strcmp(action, "resume") != 0 && strcmp(action, "remove") != 0 && strcmp(action, "delete") != 0) return false;
    if (member != NULL && !Hex(member, 16)) return false;
    (void)snprintf(path, sizeof(path), "/api/worlds/%s/host", company->id);
    (void)snprintf(body, sizeof(body), "{\"action\":\"%s\",\"member\":\"%s\"}", action, member != NULL ? member : "");
    if (!CcCompanyRequest(path, body, &json, error, capacity)) return false;
    bool ok = strcmp(action, "delete") == 0 ? CcCompanyJsonNumber(json, "$.deleted") != 0 : CompanyRead(company, json);
    free(json);
    return ok;
}
