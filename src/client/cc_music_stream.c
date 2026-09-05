#include "client/cc_music_stream.h"
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
#endif
EM_JS(int, CcMusicStreamOpen, (const char *path, const unsigned char *data, int size), {
    const bytes = data ? HEAPU8.slice(data, data + size) : FS.readFile(UTF8ToString(path));
    return Module.ccMusicPlayback.open(bytes);
});
EM_JS(int, CcMusicStreamReady, (int stream), { return Module.ccMusicPlayback.ready(stream); });
EM_JS(float, CcMusicStreamLength, (int stream), { return Module.ccMusicPlayback.length(stream); });
EM_JS(float, CcMusicStreamPosition, (int stream), { return Module.ccMusicPlayback.position(stream); });
EM_JS(void, CcMusicStreamClose, (int stream), { Module.ccMusicPlayback.close(stream); });
EM_JS(void, CcMusicStreamPause, (int stream), { Module.ccMusicPlayback.pause(stream); });
EM_JS(void, CcMusicStreamResume, (int stream), { Module.ccMusicPlayback.resume(stream); });
EM_JS(void, CcMusicStreamVolume, (int stream, float volume), { Module.ccMusicPlayback.volume(stream, volume); });
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif
