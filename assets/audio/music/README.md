# Situation music

The director ranks all 64 soundtrack titles by the current place and situation.
The catalog contains 149 takes, including the dark pixel remixes.

Road progress blends the origin and destination regions from 100/0 to 0/100.
The same rule follows a journey in either direction. Nearby work sites add a
smooth attraction within 28 world units. Named mills, mines and dungeon rooms
pull their own cues forward. Weather, night, hunger, markets, goblins, dragon
signs and local combat add theme weights.

The selector squares each relevance score and makes a weighted random choice.
It chooses a title first, then a take. Recent titles get a lower chance. A title
with several installed takes chooses a different take on its next play. The
first folk arrangements have a take weight of 0.45; the dark pixel takes use 1.
The shuffle has its own random state, so music choices preserve simulation RNG.

| Change | Timing |
| --- | --- |
| First track | 4 second fade in |
| Region or ordinary scene | 12 second crossfade after 25 seconds of play and a clear relevance change |
| Bandit attack or town alarm | 1.2 second crossfade, including during another fade |
| Danger passes | Hold combat for 8 seconds, then crossfade for 8 seconds |
| Track ends | 8 second overlap into another relevant take |
| Matching tracks become unavailable | 4 second fade to silence |

The mixer interpolates signal power and takes its square root for volume.
This keeps the combined power steady across a transition. Three bounded stream
slots let an attack interrupt an ordinary crossfade at its current volumes.
Fades use real time, including during fast travel. Losing window focus pauses
the streams and the director together.

## Audio files

The catalog and gameplay integration are ready for exported audio. Add files
with the stems in `catalog.json`, for example `03-01.ogg` and `59-01.mp3`.
The player accepts OGG, MP3, WAV and FLAC. It scans installed takes after the
first play input and streams up to three at once. Native builds check the
working directory, source assets and app resource directory. A packaged browser
build can use files mounted at the same paths in its virtual filesystem; the
audio delivery step can choose which files to mount or stream remotely.

The audio exports remain in Suno. The private download map is kept with the
soundtrack work artifacts. This catalog contains titles, themes, file stems
and lengths.

## Editing themes

Edit `catalog.json`, then run `python3 tools/music_catalog.py`.
`python3 tools/music_catalog.py --check` checks the generated C table.
`situation_music_and_fades` tests route direction, place attraction, weighted
shuffle, combat priority, interruptions and fade continuity.
