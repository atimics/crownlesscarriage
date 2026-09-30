# Hollowbarrow recovery: visible browser route

This review uses synthetic saved worlds with the same abandoned-town road and care conditions. The browser ran at a 390 × 844 touch viewport against a local shared host. `tests/stranded_recovery_browser_tests.cjs` records the commands sent by the browser.

## Result

- Thornford had zero residents, visitors, services, and food rations. The company still held two Wood. The town showed a road choice and an unavailable care action.
- A mare near foaling blocked a separate road. The visible button gave the saved reason: “A mare near foaling must remain at the stable.”
- The player used **Choose a road** and four visible road legs to reach Gloamgate. Its care action explained that it had no staffed stable.
- The player used **Board Crownless carriage → Travel → Alderwatch**. Three **Travel on** actions, with stops and resumes, reached the staffed town.
- Alderwatch offered care for five crowns, one market Wheat, and one day. The player tapped the care button. Company crowns went from 42 to 37; horse health went from 65 to 72; hunger went from 1 to 0. The saved care receipt survived a browser reload with hash `17092a618ccdc688`.
- Both road arrivals kept the company, carriage, and two Wood together. Each leg advanced the saved clock. Every recorded browser command returned HTTP 200.

## Captures

- [Blocked foaling road](foaling-road-choice.png)
- [Abandoned town and care](abandoned-town-care.png)
- [Gloamgate arrival](visible-arrival-gloamgate.png)
- [Alderwatch arrival](visible-arrival-alderwatch.png)
- [Staffed stable care](staffed-stable-care.png)

The affected player's saved-world replay and long-absence return remain part of [issue #722](https://github.com/atimics/crownlesscarriage/issues/722). The populated hungry-town case and walking custody rules also remain there and in their linked issues.
