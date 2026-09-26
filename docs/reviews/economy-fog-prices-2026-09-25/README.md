# Prices under the fog: the map case

Town economies, PR 2 (`docs/design/town-economies.md`, "Prices under the
fog"). The map case now lists the prices the company knows for the two towns
a chart depicts. Only the town the company stands in shows today's prices.
Elsewhere it shows the snapshot The Return took when the company last left
that town, with its age. A town the company has never left shows no price.

Frames are saved as 256-colour PNGs to keep the folder small. Seed and day
come from the default capture world (day 29, the company in Gloamgate).

| Frame | Command | What it shows |
| --- | --- | --- |
| `map-case-before.png` | `--capture-map-case PNG` on main | No prices anywhere in the map case. |
| `map-case-after-unknown.png` | `--capture-map-case PNG` | Gloamgate "here today" with live prices in teal. Silverwick "no prices known": the company has never left it. |
| `map-case-after-seen.png` | `--capture-map-case PNG fog` | Silverwick "seen 26 days ago" with its old prices, faded by age. The capture raises Silverwick's live bread and wheat prices after the snapshot (bread 12c, wheat 5c live); the case still shows 8c and 2c. A told shortage story adds "Since then: heard of a shortage (day 26)". |
