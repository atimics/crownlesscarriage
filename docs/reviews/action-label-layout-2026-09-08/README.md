# Action sign layout review

Partial work for #258 and #293. Based on #560 (50f8f44).

The Gloamgate road fork placed Thornford and Hollowbarrow signs on top of each other. The shared label layout now moves nearby signs to separate rows within the scene. Label height follows the text setting. Drawing and pointer selection use the same rectangles. Action signs receive space before temporary focus details. When the scene is full, the existing action cards remain available.

## Evidence

- [Crowded fork before](fork-before.png)
- [Crowded fork at 1040 by 700](fork-1040.png)
- [Crowded fork at 1280 by 700](fork-1280.png)
- [Standard text at 1040 by 620](road-1040-0.png)
- [Largest text at 1040 by 620](road-1040-2.png)
- [Standard text at 1200 by 700](road-1200-0.png)
- [Largest text at 1200 by 700](road-1200-2.png)

The fork comparison separates both destination names. The large-text captures keep names and return controls within the scene. Captures use the sizes supported by the current diagnostic modes; broader 1280 by 720, mobile, browser zoom, and fresh-player checks remain part of the parent issues.

## Validation

Strict native build and all 119 tests passed. The geometry regression checks eight crowded signs, all viewport edges, full-scene fallback, oversized labels, and invalid coordinates. The existing world-card route test now selects action signs through their laid-out centers and verifies the same journey results as action cards. Physical target tests retain their object positions. Shared fallback viewport dimensions support these headless input tests.
