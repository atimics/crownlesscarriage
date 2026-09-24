# Trade panel layout review

The 1040×620 baseline shows the town HUD and keeper speech controls crossing the trade panel heading and tabs. The updated modal hides the duplicate location HUD, moves speech controls into the upper-right margin, and places the keeper caption in a full-width footer lane.

The six-case Playwright run passed for town action controls, Company Book, and Granary trade at 1040×620, 1200×700, and 1280×720 with normal and large body/caption text. All touch action buttons remained inside the canvas. The 1040×620 large-text keeper caption has a 20-pixel measured glyph extent (bright pixel rows 545–564).

| Capture | Evidence |
| --- | --- |
| `before-1040x620-large-trade.png` | HUD, replay/skip buttons, and caption overlap the trade heading/tabs. |
| `after-1040x620-normal-trade.png` | Minimum desktop, normal text. |
| `after-1040x620-large-trade.png` | Minimum desktop, large text; 27-pixel keeper caption glyph extent. |
| `after-1200x700-normal-trade.png` | Standard desktop, normal text. |
| `after-1200x700-large-trade.png` | Standard desktop, large text. |
| `after-1280x720-normal-trade.png` | Larger desktop, normal text. |
| `after-1280x720-large-trade.png` | Larger desktop, large text. |

This slice covers the modal trade heading, speech controls, and keeper caption. Broader #293 work remains for mobile canvas sizing and reflow across the other gameplay panels.
