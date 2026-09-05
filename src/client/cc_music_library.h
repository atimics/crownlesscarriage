#ifndef CROWNLESS_MUSIC_LIBRARY_H
#define CROWNLESS_MUSIC_LIBRARY_H

#include "client/cc_music.h"
#include <stddef.h>

#define CC_MUSIC_FILE_NAME 75
#define CC_MUSIC_DOWNLOAD_LIMIT (16U * 1024U * 1024U)
#define CC_MUSIC_HOST "https://crownless-music.pages.dev"

typedef struct CcMusicLibrary {
    char file[CC_MUSIC_TAKE_COUNT][CC_MUSIC_FILE_NAME];
} CcMusicLibrary;

/* Accept only catalogued stems and content-addressed MP3 filenames. */
bool CcMusicLibraryParse(CcMusicLibrary *library, const unsigned char *data, size_t size);
bool CcMusicBundled(int take);

#endif
