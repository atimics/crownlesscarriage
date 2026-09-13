# Production capture checkpoint validation

The protocol-7 capture ran from clean implementation commit ef17775657bcb90ed495b55a2e16525ac8b81b05 with a Release runner built from this worktree. It used direct seed `0x5EED0001` and 40 years for each of eight runs: baseline/opened pilots, natural/slain-at-day-one dragon policy, and two repeats each.

All four report pairs matched. The overall manifest is complete. Its eight captures record 328 checkpoints in total: eight starting states and 320 annual states through day 14,601. The existing runner keeps its annual world validation and material-accounting report protocol.

`manifest.json` is the original capture manifest. It records binary identity, commands, report/save/stderr hashes, checkpoint days, and final state hashes. One full gzip report per matching pair is retained here. `retained-reports.json` records compressed sizes and hashes plus the decoded report hash. Each second report matched its first byte for byte. The saved databases and stderr files remain in the local capture directory named in the manifest; the documented command regenerates them.

The controlled runner tests passed all six scenarios across four test methods: success, a third-run process failure, malformed JSON, wrong checkpoint day, missing save, and mismatched repeats. They verify that a running manifest exists before every process starts and that failed runs preserve completed predecessors and all available file hashes. The CTest registration passed. The existing real-runner production JSON checks also passed, including report repeatability and save/text parity.

See `docs/production-capture.md` for reproduction and status meanings. These results validate capture bookkeeping. They provide a fresh baseline for #394 and preserve the whole-policy labels for future economy comparisons.
