# Music delivery

The public soundtrack host is https://crownless-music.pages.dev.
It currently contains 27 exported takes. The theme catalog describes 149 finished
Suno takes across 64 titles. The other 122 finished takes await export from Suno.
The generation session also had one failed remix, making 150 attempts.

The game reads `catalog.txt` when music starts. It checks again every five minutes,
or after one minute when the host is unavailable. It chooses music by the current
place, road progress, and situation. Each published take can join that shuffle.

Desktop releases contain the 27 MP3s in `assets/audio/music`. The macOS app keeps
them inside `Contents/Resources/assets/audio/music`. The player chooses its next
song after five seconds of stable play. It downloads, opens and buffers that
choice while the current song continues. A change in place or combat can replace
the prepared choice. The current score keeps its gain until the replacement is
ready. Failed files retry after a minute; other catalog entries remain available.

The player keeps at most three active fades and one prepared song. Each download
has a 16 MiB limit, a three-second connection timeout on desktop, and a 30-second
transfer timeout. Cancelling an old choice releases the download slot for its
replacement. Catalog refresh failures retain the last good catalog.

Native music uses two 48,000-frame buffers (two seconds at 48 kHz). Speech uses
two 24,000-frame buffers. Each stream fills before its first audible frame. The
music director reads playback position from the audio source, so transition
choices follow the song through a slow game frame.

The browser package contains the 27 MP3s as separate static files outside the
startup data pack. Bundled songs use that local URL first. The browser saves
these files one at a time during spare download time. A selected song takes
priority over offline warming. A matching background transfer becomes the
selected transfer. Playback can use completed bytes while storage writes its
copy. Once warming finishes, all 27 files are available offline; the set needs
122 MB of browser storage. Clearing site storage removes that copy.

Browser music plays from downloaded MP3 blobs through native media elements.
The browser handles decoding and playback separately from the game frame. A
prepared element starts silently, fills, then pauses at the beginning until its
fade is due. The C director still chooses songs, controls gains and handles
combat. Page visibility pauses active music. Closing a stream releases its
media element and blob URL. The four-element limit matches the player memory
limit. Speech and sound effects continue through the existing raylib mixer.

Published filenames use known track stems and SHA-256 names. HTTPS protects the
catalog and downloads in transit.

## Add later exports

Keep the existing 27-file offline set. Put newly exported MP3 files in a separate
staging folder using the stems in `assets/audio/music/catalog.json`. Include copies
of the existing 27 files in that folder to retain them in the hosted catalog.

```sh
python3 tools/music_host.py --audio-dir /path/to/staged-music
wrangler pages deploy out/music-site --project-name crownless-music --branch main
```

The exporter publishes the supplied MP3s, a small text catalog, a JSON catalog,
and CORS/cache headers. The public metadata contains titles, durations, file sizes,
and hashes. Audio filenames are immutable; the catalog expires after one minute.

## Checks

`music_tests` covers situation ranking and waiting for audio during fades.
`music_player_tests` covers remote loading, ready combat music, failed downloads,
and cleanup. `tests/web_music_tests.mjs` checks all 27 cached files while offline,
reconnects, request size limits, and full browser storage.
`tests/music_host_tests.py` checks the 27 local hashes and host export format.
