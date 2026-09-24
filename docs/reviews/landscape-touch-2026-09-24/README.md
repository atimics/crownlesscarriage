# Landscape touch actions

At 844×390, the semantic action list now sits beside the game canvas as a scrollable column. Action rows stay at least 44 CSS pixels high with 16px text. The game canvas remains 16:9 at 523.3×294.3 CSS pixels; the action column measures 320.7×346 CSS pixels.

The Chromium journey starts a new campaign, opens Granary Hall, buys two Bread, uses the visible Save action, reloads, and verifies the saved trade. The keeper quotes 8 crowns; the receipt changes the purse from 42 to 34 and cargo from zero to two Bread. Reload keeps 34 crowns and two Bread.

| Capture | Evidence |
| --- | --- |
| `title-844x390.png` | The visible action area beside the landscape canvas. |
| `trade-quote-844x390.png` | Granary stock and 2-Bread quote before confirmation. |
| `trade-receipt-844x390.png` | Confirmed 8-crown receipt and updated purse/cargo. |
| `town-save-844x390.png` | Visible Save action after leaving the trade panel. |
| `trade-reload-844x390.png` | Saved purse and cargo after reload. |

The browser test also checks that all listed action rows meet the 44px target and fit beside the canvas. Portrait touch layout remains covered by the existing mobile test.
