# Production integration with main

This branch integrates production PRs #500 through #518 with main at 7f198f8. It keeps the shipped schema 60 traveller state and assigns production rule versions 61 through 70. SQLite format 32 includes traveller columns, historic characters, and road stores.

The merge also retains oral reports at an unfunded scriptorium from #499. Historic simulation replay retains the earlier scribe rule.

The shipped traveller fixture uses seed 0x5eed0001 at day 1461, written by the main simulation and persistence libraries. Its original schema 60 hash is af82f2230153da41. Nine further days produce 3c8745b83278bea2. The persistence test checks both values after loading with this integration. The fixture uses SQLite DELETE journal mode so it is a standalone file.

All 87 headless tests passed, including the corrected legacy journal version cases. Full client and GitHub checks are recorded in the integration PR. Earlier production experiment reports remain evidence for their named commits; this integration changes the current version and includes traveller behavior.
