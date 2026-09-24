# Trade text reflow evidence

These WebAssembly Chromium captures use the minimum desktop viewport (1040×620) and a phone viewport (390×844). They follow one campaign through two Bread purchases.

The first offer shows 8 crowns for 2 Bread. Its receipt charges 8 crowns, leaving 34 crowns and 2 Bread. The confirmed 8-crown offer stays with the receipt and the Buy action is disabled until the offer changes. After reload, the market quotes 10 crowns for the next 2 Bread. That purchase charges 10 crowns. The second save and reload leave 24 crowns and 4 Bread. `results.json` records both viewports and transactions.

Measured rendered glyph height from the desktop captures:

- The gold first-offer price is 16 pixels high in `desktop-1040x620-before.png`.
- The first receipt is 16 pixels high in `desktop-1040x620-receipt.png`.
- The follow-up quote and receipt show the same reflow in the next two desktop captures.
- The phone touch-detail text measures 15–16 pixels high in the phone captures; the browser journey also checks its computed 16px font size.

For the pixel measurement, the desktop price crop used x=530–900 and y=205–250 with a gold-pixel mask (`r>120`, `g>90`, `b>35`, `r-g<80`). The receipt crop used x=140–900 and y=440–475 with a light-text mask (`r>110`, `g>100`, `b>90`). The reported height is the contiguous row span containing at least three masked pixels. The phone detail used the same light-text mask over x=16–374 and y=285–335.

The existing touch controls retain their 16px phone text and complete both purchases, saves, and reloads.
