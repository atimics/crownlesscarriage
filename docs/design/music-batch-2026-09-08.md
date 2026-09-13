# September music batch

The batch uses 20 Suno song unlocks and adds 50 minutes and 9 seconds to the
hosted mix. It supplies one take for each of the six towns in three moods.
The shared hunger and food-relief themes fill the last two slots.

| File | Theme |
| --- | --- |
| 61-01.mp3 | The Last Sack of Seed |
| 62-01.mp3 | Bread at Last |
| 65-01.mp3 | Thornford - Flour on the Windowsill |
| 66-01.mp3 | Thornford - Flour on the Windowsill - Shortage |
| 67-01.mp3 | Thornford - Flour on the Windowsill - Recovery |
| 68-01.mp3 | Gloamgate - Small Change, Tall Stories |
| 69-01.mp3 | Gloamgate - Small Change, Tall Stories - Shortage |
| 70-01.mp3 | Gloamgate - Small Change, Tall Stories - Recovery |
| 71-01.mp3 | Alderwatch - Boots on the Old Bridge |
| 72-01.mp3 | Alderwatch - Boots on the Old Bridge - Shortage |
| 73-01.mp3 | Alderwatch - Boots on the Old Bridge - Recovery |
| 74-01.mp3 | Silverwick - Shift Change in Blue |
| 75-01.mp3 | Silverwick - Shift Change in Blue - Shortage |
| 76-01.mp3 | Silverwick - Shift Change in Blue - Recovery |
| 77-01.mp3 | Rosespire - Petals in the Clockwork |
| 78-01.mp3 | Rosespire - Petals in the Clockwork - Shortage |
| 79-01.mp3 | Rosespire - Petals in the Clockwork - Recovery |
| 80-01.mp3 | Hollowbarrow - One Lantern Left |
| 81-01.mp3 | Hollowbarrow - One Lantern Left - Shortage |
| 82-01.mp3 | Hollowbarrow - One Lantern Left - Recovery |

## How the batch joins play

The current director already selects these catalog entries by town and mood.
The hosted catalog grows from 27 to 47 takes. Players pick up the catalog when
music starts or on its five-minute refresh. Town hunger selects shortage at 40.
Recent food relief selects recovery below that threshold. Everyday music applies
otherwise. Existing cues return through the title shuffle and recent-title weights.

`hosted.json` records each exported file and its fingerprint. The host builder
combines the bundled files and the hosted folder. The audio budget allows 20
hosted files and 96 MiB alongside the existing 27-file offline set.

## Next themes

The next export batch should give named interiors and work sites their own
music: Coach Court, Delvers' Lodge, Crown Palace, Rose Cloister, Nine Furrows,
Fellside Quarry, Crown Forge, Highroad Bakehouse and Abbey Fold. Their catalog
entries already exist. Dungeon rooms can follow: Flooded Turntable, Chain Bridge,
Hall of Ash Clerks, Ledger Vault, Stoneback Nursery, Old Smuggler Cut, Cinder
Market, Red Cap Barracks and Tribute Lift.

For new compositions, give Wyrm, living dragon and Dracolich encounters separate
motifs, then arrange each for approach, direct encounter and aftermath. Match
those arrangements to visible encounter states when adding them to the director.

## Checks

All five music checks pass: situation selection and fades, streamed playback,
browser download and cache, generated metadata, and the bundle/host contract.
All 20 exports match their catalog durations within two seconds. Browser audio
decoded all 20 full files and confirmed native playback advances. The browser
check is repeatable through `tests/fixtures/music-batch.html` on a local server.
The host test checks the exact 20-file set, file fingerprints, all 47 published
stems, and duplicate stem rejection. This is a technical playback check; musical
balance can be tuned during play.
