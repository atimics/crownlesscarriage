# Music delivery

The public soundtrack host is https://crownless-music.pages.dev.
It currently contains 27 exported takes. The game catalog describes 185 finished
Suno takes across 82 cues: the original 149 takes plus 36 town arrangements.
The other 158 finished takes await export from Suno. The existing 27 MP3s are
bundled, hosted and playable today. Registered takes become available when their
audio files are installed or added to the host catalog.

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

## Town arrangements

Each town has two everyday takes, two shortage takes and two recovery takes.
The new cues follow the exact town name on the surface. Hunger of 40 or more
selects shortage, matching the game's visible hungry condition. Below 40 hunger,
a positive food-relief event at that town selects recovery for the delivery day
and the next two days. Everyday applies at other times. Food-relief events also
bring the existing shared relief music into the shuffle.

| Town | Everyday stems | Shortage stems | Recovery stems |
| --- | --- | --- | --- |
| Thornford | 65-01, 65-02 | 66-01, 66-02 | 67-01, 67-02 |
| Gloamgate | 68-01, 68-02 | 69-01, 69-02 | 70-01, 70-02 |
| Alderwatch | 71-01, 71-02 | 72-01, 72-02 | 73-01, 73-02 |
| Silverwick | 74-01, 74-02 | 75-01, 75-02 | 76-01, 76-02 |
| Rosespire | 77-01, 77-02 | 78-01, 78-02 | 79-01, 79-02 |
| Hollowbarrow | 80-01, 80-02 | 81-01, 81-02 | 82-01, 82-02 |

A matching arrangement receives twice the normal relevance score. With the new
pair and an old town cue available, this gives the new arrangement about 80% of
a fresh draw. The usual recent-title penalty brings the old cue back into the
mix. Extra takes affect performance choice within a cue. They share that cue's
chance. The current library covers every mood while the new exports arrive.
Roads, caves, dungeon rooms, combat and loss retain their own selection rules.

The catalog generator writes both the C table and its count header. Its limits
follow the player's two-digit stems and 16 KiB remote manifest. All 185 entries
fit in a 13,892-byte text catalog. The offline bundle remains a separate 27-file
set; later town exports can join the online library with the mapping above.

## Checks

`music_tests` covers each town and mood, relief expiry, current-file fallback,
weighted returns, combat, the full 185-entry manifest and waiting during fades.
`music_player_tests` covers early loading, combat preemption, damaged files,
playback clock changes, failed downloads and cleanup. `tests/web_music_tests.mjs` checks all 27 cached files while offline,
reconnects, request size limits, storage delays and download priority.
`tests/music_browser_tests.cjs` measures a continuous signal through a 700 ms
page stall. It also checks prepared playback, pause, resume, the four-stream
limit, damaged files and cleanup. The built-game browser check confirms that
the WebAssembly player starts native browser music.
`tests/music_host_tests.py` checks the 27 local hashes and host export format.
