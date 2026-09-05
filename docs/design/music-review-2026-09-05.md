# Music review and town theme trial

Review base: `e02b647`, fetched from main on 5 September 2026.

Crownless has a working music director and a large Suno library. The existing
library and its repetition are part of the intended score. Six clear town themes
will anchor that library through weighted returns. Three kingdom sound families
can connect those towns. Wyrm, Lich and dragon encounters need their own musical
identities, each distinct from the location score and from the other encounters.

## What has reached the game

The original Suno workspace displays 149 songs. The saved generation record and
the game catalog agree on 149 finished takes across 64 titles. The record includes
128 original takes, 21 successful pixel remixes and one failed remix attempt.

The live [host catalog](https://crownless-music.pages.dev/catalog.json) was read
on 5 September 2026. Its 27 stems, sizes and hashes match the offline manifest.
The SHA-256 of every bundled MP3 also matches that manifest.

| Measure | Count |
| --- | ---: |
| Finished takes in the original Suno set | 149 |
| Takes in the compiled game catalog | 149 |
| Takes bundled in the repository | 27 (18.1%) |
| Takes listed by the live music host | 27 |
| Titles represented by those files | 26 of 64 (40.6%) |
| Finished takes awaiting export | 122 |
| Titles awaiting their first export | 38 |
| Total catalog duration | 7 h 46 m 49 s |
| Bundled duration, from catalog metadata | 1 h 22 m 54 s |
| Bundled MP3 size | 122,444,378 bytes (122.4 MB) |

Six imported files are pixel remixes of the first folk cues. The other 21 are
original takes. Many later original takes were already written for dark pixel
electronica, including Silverwick, Rosespire and Hollowbarrow.

The imported stems are:

```text
01-05 03-03 03-05 04-01 05-01 06-01 07-01 08-05 09-01
11-01 12-05 15-01 16-03 19-01 20-01 21-01 23-01 24-01
27-01 28-01 39-01 46-01 52-01 53-01 59-01 60-01 63-01
```

Every town has one imported town take and one regional travel take. The set also
covers rain, night, camp, the mill, mine, dungeon, goblins, dragon, bandits, siege
and loss. The remaining exports include all three regional night cues, inns, many
work sites, hunger and relief, most dungeon rooms and several combat themes.

The original prompts already give towns different keys, meters and lead sounds.
The early folk tracks and later dark pixel tracks share broad palettes. The new
trial pushes identity through melody shape, groove and lead instrument together.

## How music follows play

`LocalMusicContext` in `src/client/main.c` builds the scene from the same local
state used for travel, interiors, encounters and weather. It passes that scene
to `CcMusicContextFor` in `src/client/cc_music.c`.

1. Towns contribute their economic type and hunger. An exact town name boosts
   its named cue. Road progress blends the origin and destination types.
2. Night, rain, markets, camp, caves, room names, dragon signs and combat add
   relevance. Nearby road sites contribute within 28 world units, using the
   renderer's site positions and a smooth distance curve.
3. The director squares each score, applies recent-title penalties, chooses a
   title, then chooses a take. Extra takes give a title more performances while
   keeping its title-level chance stable. The last take is skipped when another
   take of that title is available. Music uses its own random state.
4. The player opens a local file or downloads the selected MP3. The current
   music continues during the download. Up to three streams support overlapping
   fades and a sudden combat interruption.

The score is `primary theme + 0.22 * secondary theme + 3 * named-place weight`.
A regional cue then receives a multiplier of `0.03 + 2 * region weight`. The six
regions mean farming, mining, market, fortress, capital and dungeon frontier.
Kingdom identity and the nine political factions are the next context layer.

A probe against the current C library used seed 17, the bundled set and a fresh
director in each quiet daytime town. Each town's own cue had a 99.847% chance on
the first draw. Weather, nearby sites and recent-title history change that
figure during play. Each town currently has one available take of its own cue.
These returns give the town a familiar sound. The new weighting should use that
recognition while keeping the wider library active around a few strong anchors.

The review also found that the town's name boost carried into underground scenes.
At the review base, a quiet dragon cave near Silverwick gave its town cue 81.960%
of a fresh draw and the dragon cue 18.040%. This change scopes the town-name boost
to surface scenes. Tests cover entry to goblin caves, dragon caves and dungeon
expeditions, followed by a return to town. Local room and cave cues can then lead
the underground score.

| Event | Transition |
| --- | --- |
| First track | 4 second fade in |
| Ordinary scene change | 12 second fade after 25 seconds and a relevance change |
| Combat begins | 1.2 second fade |
| Combat ends | 8 second hold, then 8 second fade |
| Track nears its end | 8 second overlap |

Fades run on real time. Their gains follow equal power. Song endings are handled
by timed overlap and stream looping. Export edits should give them clean starts
and endings. Beat-aligned transitions and separate instrument layers would be
later extensions.

Music begins after play input and audio-device readiness. Full sound mode includes
music; the sound mode and window focus pause or resume its streams and director.
Dialogue reduces its target gain to 36%, with a smooth return. The player applies
a final 0.55 volume scale. Song loudness currently comes from the exported file,
so a common loudness pass will help the new set sit well under speech and effects.

## Delivery and catalog constraints

Desktop bundles include all 27 MP3s. The web build ships them as separate files
outside its startup pack. Once music starts, the browser fills its music cache
one file at a time. Offline coverage reaches the full set when that copy finishes.

The player refreshes `catalog.txt` after five minutes, or after one minute on a
host failure. Downloads have a 16 MiB limit and a 30 second transfer timeout.
Desktop also has a three second connection timeout. The player keeps at most the
three active fade tracks and one pending download in memory.

The host adds exports of already-known stems through `tools/music_host.py`.
New titles or takes also need a client catalog update: `CcMusicLibraryParse`
resolves every remote stem against the compiled take table. The public host stores
availability and filenames; musical tags and weights are compiled into the game.
Desktop starts with its bundled copy when one exists for a stem. Stable new stems
will give new compositions clear identities across desktop and browser caches.

The current generator expects exactly 64 cues and 149 takes, with variants 1–7.
Alderwatch already uses variants 1–7. Adding new compositions as new cues after
the existing 64 keeps every original cue and take available. Importing six chosen
trial takes would make 70 cues and 155 takes. Both trial takes per town would make
70 cues and 161 takes.

The offline set has a separate 27-file / 128 MiB cap. Its file limit is also in
the browser cache warmer and bundle checks. An initial online trial can add six
chosen tracks while retaining the 27-file offline set. Bundling them later needs
an explicit update to that set, its cache manifest and its size budget.

## Six town themes and three kingdom families

The six town names are fixed in the current world generator. They start in three
pairs. Kingdom names vary with the world seed; the same family should follow the
kingdom's stable identity in a saved game.

| Family | Starting towns | Example kingdom names | Shared sound |
| --- | --- | --- | --- |
| Road | Thornford, Gloamgate | Alder Concord / Willow Republic / Moss Compact | Wood, breath, plucked strings, warm swing |
| Iron | Alderwatch, Silverwick | Ember Crown / Ashen Throne / Rose Dominion | Muted metal, low strings, measured drum pulses |
| Deep | Rosespire, Hollowbarrow | Star Oath / Indigo March / Silver Covenant | Glass bells, long space, suspended harmony |

Each kingdom has Court, Factors and Commons factions in the simulation. These
are nine named factions in total. Give each its own short tune within its family:
Court uses formal phrases, Factors use interlocking patterns, and Commons use
plain singable phrases. Family sound ties them together. Town melodies stay with
their places; a change of ruler can introduce the new ruler's theme at court and
in related events. Goblin groups and dragons can have additional identities as
their encounters receive a later pass.

The first trial produced twelve finished Suno v5.5 takes, two per town, totalling
28 minutes 3 seconds. Each prompt sets a different melodic shape. The shared
thread is warm 16-bit sampler colour, gentle tape movement, modest dynamics and
room for dialogue.

| Town | Trial | Pulse and key | Lead |
| --- | --- | --- | --- |
| Thornford | Flour on the Windowsill | Pastoral folktronica, 76 BPM, 6/8, G major | Wooden flute, thumb piano, lute |
| Gloamgate | Small Change, Tall Stories | Swung market groove, 96 BPM, A mixolydian | Dulcimer and muted FM clarinet |
| Alderwatch | Boots on the Old Bridge | Quiet march, 84 BPM, D aeolian | Muted horn and grainy cello |
| Silverwick | Shift Change in Blue | Industrial trip-hop, 72 BPM, C dorian | Electric piano and tuned iron |
| Rosespire | Petals in the Clockwork | Baroque dream waltz, 90 BPM, E-flat lydian | Harpsichord and FM celesta |
| Hollowbarrow | One Lantern Left | Warm dungeon ambient, 60 BPM, B minor | Ocarina, marimba and bell echoes |

The exact prompts are in [town-music-trial.json](town-music-trial.json). Keys,
tempos, meters and motif shapes are composition requests. A listening review will
establish how closely each take follows them. Suno source links stay with the
private listening artifacts, following the existing download-map convention.

## Listening and integration

Compare each pair with its existing town take. Judge the first 30 seconds for a
recognisable tune and clear place identity, then hear the full arrangement for
fatigue, dialogue space, transitions and ending quality. Hear the six winners in
sequence to judge how well they share the fantasy world. A blind town-matching
pass can check whether listeners can place each tune.

The trial set remains available for this listening pass. The next import should
add one winner per town as its primary arrival theme and retain the current town
cue as an alternate. A primary-cue preference belongs at title selection: the
existing take weight only changes performances within the same title.

Use these rules for the first weighting pass:

- A town's ready anchor leads an arrival and returns during a longer stay.
- The existing library fills the space between those returns. Region, activity,
  weather and mood continue to guide those choices.
- Familiar repeats remain welcome. Judge the weighting by recognition and place
  identity. Give the anchor enough weight to recover after the recent-title
  penalty, then tune the balance through listening.
- Returning to a town brings back its established melody. Its alternate takes
  provide different performances of that familiar theme.
- A named supernatural encounter leads with its own theme. Its identity chooses
  the cue family before the ordinary scene shuffle. Encounter phase then chooses
  approach, audience, combat or aftermath within that family.

Combat keeps its quick transition and recovery. Existing library growth can
continue through exports of regional nights, inns, work sites and dungeon rooms.
Town anchors come first; the Wyrm, Lich and dragon identities are the next music
pass. Kingdom and faction themes can grow from those results.

## Wyrm, Lich and dragon identities

These encounters should be recognisable in their first few seconds. Each gets a
fresh motif, its own instrument palette, rhythmic language and sense of space.
Their link to the pixel world can be the grain of the samples. Their composition
can reach far beyond the town score's relaxed feel.

| Identity | Musical direction | Recognisable signature |
| --- | --- | --- |
| Deep Wyrm | Primordial ritual doom; immense bass, bowed metal and stone-like percussion | A vast low note, a grinding half-step rise, then a long fall; slow uneven breaths |
| Lich | Cold ritual counterpoint; pipe organ, brittle keys and a precise skeletal pulse | A clipped high figure that answers itself in another register; uncanny precision |
| Dragon | Predatory grandeur; raw brass, rushing strings and heavy wingbeat drums | A sudden wide upward leap and a commanding descending answer; force and movement |

The Deep Wyrm direction follows the existing stage in `CcDragonLifeStage`:
centuries bind the creature, hoard and mountain. The music context can use that
stage together with a direct encounter and its phase. Dragon signs during travel
can keep their current supporting cues; an audience or fight earns the full
encounter identity. Lich activation will bind to the intended encounter state
when its creature identity is specified.

Each named creature can own its own motif within its encounter family. A later
fight arrangement should retain that creature's motif so recognition survives
the change in intensity. The existing dragon and combat tracks remain useful
for signs, approaches, support and aftermath as their tags and scene fit allow.

## Verification

The new underground-scene regression failed on the review base. After the town
boost fix, the strict Debug build of `music_tests` and `music_player_tests` passed. All five
music CTest checks passed: situation selection and fades; streamed playback and
fallback; browser download and cache; generated catalog; bundled files and host
format. The quiet-town probe used the current compiled C scoring code. File
hashes were checked locally against the offline manifest and the live host's
metadata. All twelve Suno takes show finished durations. The browser decoded and
played a Thornford take. New composition quality is reserved for the listening pass.

The original integration was merged in PR #295. Hosted delivery and the offline
set were merged in PR #306. The current code and live catalog were read directly
for this review.
