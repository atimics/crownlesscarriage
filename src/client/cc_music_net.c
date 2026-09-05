#include "client/cc_music_net.h"
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
#endif
EM_JS(int, WebStart, (const char *url, int limit), {
    return Module.ccMusicNet.start(UTF8ToString(url), limit) ? 1 : 0;
});
EM_JS(int, WebPoll, (unsigned char **data, size_t *size), {
    const request = Module.ccMusicNet.request;
    if (!request || !request.status) return 0;
    Module.ccMusicNet.request = null;
    if (request.status < 0) return -1;
    const pointer = _malloc(request.data.length);
    if (!pointer) return -1;
    HEAPU8.set(request.data, pointer);
    HEAPU32[data >> 2] = pointer;
    HEAPU32[size >> 2] = request.data.length;
    return 1;
});
EM_JS(void, WebShutdown, (), { Module.ccMusicNet.stop(); });
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
bool CcMusicNetStart(const char *url, size_t limit) { return WebStart(url, (int)limit) != 0; }
int CcMusicNetPoll(unsigned char **data, size_t *size) { return WebPoll(data, size); }
void CcMusicNetShutdown(void) { WebShutdown(); }
#else
#include <curl/curl.h>
#include <pthread.h>
#include <stdatomic.h>

static struct {
    pthread_t worker;
    atomic_int status;
    atomic_bool cancel;
    bool active;
    bool initialized;
    char url[512];
    unsigned char *data;
    size_t size, limit;
} net;

static size_t Receive(void *bytes, size_t width, size_t count, void *unused)
{
    (void)unused;
    if (width != 0 && count > (net.limit - net.size) / width) return 0;
    size_t length = width * count;
    unsigned char *next = realloc(net.data, net.size + length + 1U);
    if (next == NULL) return 0;
    net.data = next;
    memcpy(next + net.size, bytes, length);
    net.size += length;
    next[net.size] = 0;
    return length;
}

static int Progress(void *unused, curl_off_t total, curl_off_t now,
                    curl_off_t upload_total, curl_off_t uploaded)
{
    (void)unused; (void)total; (void)now; (void)upload_total; (void)uploaded;
    return atomic_load(&net.cancel) ? 1 : 0;
}

static void *Download(void *unused)
{
    (void)unused;
    CURL *curl = curl_easy_init();
    bool ok = false;
    if (curl != NULL) {
        (void)curl_easy_setopt(curl, CURLOPT_URL, net.url);
        (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Receive);
        (void)curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        (void)curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        (void)curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        (void)curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Progress);
        (void)curl_easy_setopt(curl, CURLOPT_USERAGENT, "Crownless-Music/1");
        CURLcode result = curl_easy_perform(curl);
        long status = 0;
        (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        ok = result == CURLE_OK && status == 200 && net.size > 0;
        curl_easy_cleanup(curl);
    }
    atomic_store(&net.status, ok ? 1 : -1);
    return NULL;
}

bool CcMusicNetStart(const char *url, size_t limit)
{
    if (net.active || url == NULL || strlen(url) >= sizeof(net.url) || limit == 0) return false;
    if (!net.initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return false;
        net.initialized = true;
    }
    memcpy(net.url, url, strlen(url) + 1U);
    net.limit = limit;
    net.size = 0;
    atomic_store(&net.cancel, false);
    atomic_store(&net.status, 0);
    net.active = pthread_create(&net.worker, NULL, Download, NULL) == 0;
    return net.active;
}

int CcMusicNetPoll(unsigned char **data, size_t *size)
{
    if (!net.active) return 0;
    int status = atomic_load(&net.status);
    if (status == 0) return 0;
    (void)pthread_join(net.worker, NULL);
    net.active = false;
    if (status > 0) { *data = net.data; *size = net.size; }
    else free(net.data);
    net.data = NULL;
    net.size = 0;
    return status;
}

void CcMusicNetShutdown(void)
{
    if (net.active) {
        atomic_store(&net.cancel, true);
        (void)pthread_join(net.worker, NULL);
    }
    free(net.data);
    if (net.initialized) curl_global_cleanup();
    memset(&net, 0, sizeof(net));
}
#endif
