#include "client/cc_voice_net.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
#endif
EM_JS(int, VoiceWebStart, (const char *json), {
    return Module.ccVoiceNet.start(JSON.parse(UTF8ToString(json))) ? 1 : 0;
});
EM_JS(int, VoiceWebBusy, (), { return Module.ccVoiceNet.request ? 1 : 0; });
EM_JS(int, VoiceWebPoll, (unsigned char **data, size_t *size), {
    const result = Module.ccVoiceNet.take();
    if (!result) return 0;
    if (result.status < 0) return -1;
    const pointer = _malloc(result.data.length);
    if (!pointer) return -1;
    HEAPU8.set(result.data, pointer);
    HEAPU32[data >> 2] = pointer;
    HEAPU32[size >> 2] = result.data.length;
    return 1;
});
EM_JS(void, VoiceWebCancel, (), { Module.ccVoiceNet.cancel(); });
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
bool CcVoiceNetStart(const CcSpeech *speech)
{
    char json[CC_SPEECH_JSON_CAPACITY];
    return CcSpeechJson(speech, json, sizeof(json)) && VoiceWebStart(json) != 0;
}
bool CcVoiceNetBusy(void) { return VoiceWebBusy() != 0; }
int CcVoiceNetPoll(unsigned char **data, size_t *size) { return VoiceWebPoll(data, size); }
void CcVoiceNetCancel(void) { VoiceWebCancel(); }
void CcVoiceNetShutdown(void) { VoiceWebCancel(); }
#else
#include <curl/curl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

static struct {
    pthread_t worker;
    atomic_bool cancel;
    atomic_int status;
    bool active, initialized;
    char base[512];
    char key[17];
    char json[CC_SPEECH_JSON_CAPACITY];
    unsigned char *data;
    size_t size;
} voice_net;

static size_t ReceiveVoice(void *bytes, size_t width, size_t count, void *unused)
{
    (void)unused;
    if (width != 0 && count > (CC_VOICE_DOWNLOAD_LIMIT - voice_net.size) / width) return 0;
    size_t length = width * count;
    unsigned char *next = realloc(voice_net.data, voice_net.size + length);
    if (next == NULL) return 0;
    voice_net.data = next;
    memcpy(next + voice_net.size, bytes, length);
    voice_net.size += length;
    return length;
}

static int VoiceProgress(void *unused, curl_off_t total, curl_off_t now,
                         curl_off_t upload_total, curl_off_t uploaded)
{
    (void)unused; (void)total; (void)now; (void)upload_total; (void)uploaded;
    return atomic_load(&voice_net.cancel) ? 1 : 0;
}

static long VoiceRequest(CURL *curl, bool post)
{
    char url[560];
    if (post) (void)snprintf(url, sizeof(url), "%s/v1/speech", voice_net.base);
    else (void)snprintf(url, sizeof(url), "%s/v1/speech/%s", voice_net.base, voice_net.key);
    free(voice_net.data);
    voice_net.data = NULL;
    voice_net.size = 0;
    curl_easy_reset(curl);
    (void)curl_easy_setopt(curl, CURLOPT_URL, url);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveVoice);
    (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    (void)curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    (void)curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    (void)curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, VoiceProgress);
    (void)curl_easy_setopt(curl, CURLOPT_USERAGENT, "Crownless-Voice/1");
    struct curl_slist *headers = NULL;
    if (post) {
        headers = curl_slist_append(NULL, "Content-Type: application/json");
        if (headers == NULL) return 0;
        (void)curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        (void)curl_easy_setopt(curl, CURLOPT_POSTFIELDS, voice_net.json);
    }
    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    if (result == CURLE_OK) (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    return status;
}

static double MonotonicSeconds(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void *VoiceDownload(void *unused)
{
    (void)unused;
    CURL *curl = curl_easy_init();
    bool ready = false;
    if (curl != NULL) {
        double deadline = MonotonicSeconds() + 45.0;
        long status = VoiceRequest(curl, true);
        while ((status == 200 || status == 202) && !atomic_load(&voice_net.cancel) &&
               MonotonicSeconds() < deadline) {
            status = VoiceRequest(curl, false);
            if (status == 200) {
                ready = voice_net.size >= 44 && memcmp(voice_net.data, "RIFF", 4) == 0 &&
                    memcmp(voice_net.data + 8, "WAVE", 4) == 0;
                break;
            }
            if (status != 202) break;
            struct timespec pause = {.tv_sec = 0, .tv_nsec = 250000000};
            (void)nanosleep(&pause, NULL);
        }
        curl_easy_cleanup(curl);
    }
    atomic_store(&voice_net.status, ready && !atomic_load(&voice_net.cancel) ? 1 : -1);
    return NULL;
}

bool CcVoiceNetStart(const CcSpeech *speech)
{
    if (voice_net.active || !CcSpeechJson(speech, voice_net.json, sizeof(voice_net.json))) return false;
    const char *base = getenv("CROWNLESS_VOICE_URL");
    if (base == NULL) base = "http://127.0.0.1:8766";
    if (base[0] == '\0' || strlen(base) >= sizeof(voice_net.base) ||
        (strncmp(base, "http://", 7) != 0 && strncmp(base, "https://", 8) != 0)) return false;
    if (!voice_net.initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return false;
        voice_net.initialized = true;
    }
    (void)snprintf(voice_net.base, sizeof(voice_net.base), "%s", base);
    size_t length = strlen(voice_net.base);
    if (length > 0 && voice_net.base[length - 1] == '/') voice_net.base[length - 1] = '\0';
    (void)snprintf(voice_net.key, sizeof(voice_net.key), "%016" PRIx64, speech->audio_key);
    atomic_store(&voice_net.cancel, false);
    atomic_store(&voice_net.status, 0);
    if (pthread_create(&voice_net.worker, NULL, VoiceDownload, NULL) != 0) return false;
    voice_net.active = true;
    return true;
}

bool CcVoiceNetBusy(void) { return voice_net.active; }
void CcVoiceNetCancel(void) { atomic_store(&voice_net.cancel, true); }

int CcVoiceNetPoll(unsigned char **data, size_t *size)
{
    if (!voice_net.active) return 0;
    int status = atomic_load(&voice_net.status);
    if (status == 0) return 0;
    (void)pthread_join(voice_net.worker, NULL);
    voice_net.active = false;
    if (atomic_load(&voice_net.cancel)) status = -1;
    if (status > 0) {
        *data = voice_net.data;
        *size = voice_net.size;
    } else free(voice_net.data);
    voice_net.data = NULL;
    voice_net.size = 0;
    return status;
}

void CcVoiceNetShutdown(void)
{
    CcVoiceNetCancel();
    if (voice_net.active) {
        (void)pthread_join(voice_net.worker, NULL);
        voice_net.active = false;
    }
    free(voice_net.data);
    voice_net.data = NULL;
    voice_net.size = 0;
}
#endif
