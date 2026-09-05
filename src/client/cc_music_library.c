#include "client/cc_music_library.h"
#include <stdio.h>
#include <string.h>

static const char *bundled[] = {
#include "client/cc_music_offline.inc"
};

static int TakeForStem(const char *stem)
{
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        char expected[16];
        (void)snprintf(expected, sizeof(expected), "%02d-%02d",
                       cc_music_takes[i].cue + 1, cc_music_takes[i].variant);
        if (memcmp(stem, expected, 5) == 0) return i;
    }
    return -1;
}

bool CcMusicBundled(int take)
{
    for (size_t i = 0; i < sizeof(bundled) / sizeof(bundled[0]); ++i)
        if (TakeForStem(bundled[i]) == take) return true;
    return false;
}

bool CcMusicLibraryParse(CcMusicLibrary *library, const unsigned char *data, size_t size)
{
    static const char header[] = "CROWNLESS_MUSIC 1\n";
    if (library == NULL || data == NULL || size < sizeof(header) - 1 ||
        size > 16384U || memcmp(data, header, sizeof(header) - 1) != 0) return false;
    CcMusicLibrary parsed = {0};
    size_t pos = sizeof(header) - 1;
    while (pos < size) {
        /* 5 stem + separator + 64 hash + .mp3 + newline. */
        if (size - pos < 75U || data[pos + 74U] != '\n') return false;
        const char *name = (const char *)data + pos;
        int take = TakeForStem(name);
        if (take < 0 || parsed.file[take][0] != '\0' || name[5] != '-' ||
            memcmp(name + 70, ".mp3", 4) != 0) return false;
        for (int i = 6; i < 70; ++i)
            if (!((name[i] >= '0' && name[i] <= '9') ||
                  (name[i] >= 'a' && name[i] <= 'f'))) return false;
        memcpy(parsed.file[take], name, 74);
        pos += 75U;
    }
    *library = parsed;
    return true;
}
