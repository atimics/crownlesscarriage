# Public two-browser shared journey, 30 September 2026 UTC

The deployed shared host served revision `8216235febbd11f2c31d86e6d55b8251c1880a07`. Its health check reported `ready` and zero worlds needing recovery before and after the run. The [Web game release run](https://github.com/atimics/crownlesscarriage/actions/runs/36668348712) passed the shared world container, browser checks, and public deploy jobs for this revision.

Two separate Chromium browser contexts joined one synthetic world. Mara used a desktop viewport, and Bren used a phone viewport. The world ID was `38d7f05478a02bf97c258d398f613991` with seed `3232176798`. The [receipt](shared-road-receipt.json) records both member IDs, the route, saved hashes, request times, and owner deletion.

## Journey

1. Both browsers showed the joined crew. They drove from Thornford toward Gloamgate. A road choice kept the same saved hash across a browser reload.
2. At the afternoon stop, both browsers showed hash `32b5beedb8728ddb` at day 1, minute 498. The [stop capture](shared-afternoon-stop.png) shows the visible Travel and Road options controls.
3. The owner chose **Camp until morning**. The shared clock advanced 480 minutes to minute 978 while the road progress stayed at 820. Both browsers showed hash `f75182d31ac6301a`. The [camp capture](shared-road-camp.png) shows the choice.
4. Bren reloaded and reconnected in a new browser page. Both browsers still showed the camp hash and the same route state. The [reconnected phone capture](shared-reconnected-camp.png) shows Travel available.
5. The owner chose Travel. The first `resume_travel` command with sequence 14 returned `accepted: true`, `duplicate: false`. Replaying that exact command returned `accepted: true`, `duplicate: true` with sequence 14. The host action revision and journey state stayed unchanged by the replay.
6. Both browsers and the host agreed on arrival hash `483834a5525eb8da` at Gloamgate. The [arrival capture](shared-arrival.png) shows the destination. The owner deleted the synthetic world; the host returned `deleted: true`.

## Request times

The public test made 20 direct state reads from each browser after joining. Normal browser polling and road play added more reads. Times below run from the browser request event through response completion.

| Browser requests | Count | Median | 95th percentile | Slowest |
| --- | ---: | ---: | ---: | ---: |
| Owner state reads | 126 | 72 ms | 190 ms | 1,333 ms |
| Crew state reads | 87 | 86 ms | 206 ms | 1,234 ms |
| Shared commands | 20 | 176 ms | 286 ms | 302 ms |

Every recorded request in these groups completed without an HTTP error or browser request failure. State reads stayed inside the 12-second client limit. Commands stayed inside the 10-second client limit. This sample covers one synthetic world on one deployed revision.

## Repeat the check

`tests/coop_browser_tests.cjs` now records state and command times when `CC_COOP_ORIGIN` points at a deployed host. A host operator supplies a fresh single-use world pass. The test creates a synthetic world, runs the two-browser journey, writes `browser-results/shared-road-receipt.json`, and deletes the world.

```sh
CC_COOP_ORIGIN=https://crownless.ratimics.com \
CC_COOP_WORLD_PASS=<single-use-pass> \
CC_PLAYWRIGHT_MODULE=<path-to-playwright> \
node tests/coop_browser_tests.cjs
```

The live timed run used a temporary copy of this test while the timing code was prepared. The checked-in test carries the same measurement and acceptance checks. A prior timing run finished its journey and deleted its world; its first timing check counted a state request canceled by browser navigation. The final run records navigation cancellations separately.
