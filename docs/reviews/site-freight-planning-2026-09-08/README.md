# Site freight planning

Issue #391 needs physical transport between production sites and towns. This slice supplies a shared preview for an idle royal carriage at a site's home town. It builds on the site stores, recipes and route positions in PRs #505–#507.

The planner uses actual town and site stocks. It collects finished goods first and keeps one working Tool at a forge. Supply proposals cover the recipe's reserves and two batches, with Tools first. Town food and wheat keep a six-week buffer as well as their ordinary reserve. Loads fit the carriage, the available weekly road capacity, the destination store and the maximum town stock. Closed roads, weak or closed sites, busy carriages and towns belonging to another crown receive explicit gate results.

The result contains the real town, site and route IDs, the good, quantity, and both journey times. Home-town identity selects the service relationship; each store retains its own contents. Pickup quantity describes the proposed return cargo. Road capacity is a snapshot at preview time. A future loading operation must re-check stock, space and capacity at that moment.

Use `crownless_sim_runner --seed 0x5eed0001 --years 40 --report-every 1 --site-freight` to print ready proposals at report times. Each row starts with `site-freight-preview`. These are engine diagnostics. Actual site dispatch, cargo movement, arrival and return handling remain the next implementation slice under #391.

Validation:

- Strict Debug build and 77 headless tests.
- The focused test checks supply quantities, food reserves, working Tools, store space, town stock headroom, fresh and stale weekly road usage, route closure, site condition, carriage custody, travel times and state-hash purity.
- The complete 40-year annual report for seed `0x5eed0001` matches with the preview option enabled. That unmodified world yields zero ready proposals at its annual report times. The focused fixture explicitly opens and funds sites to exercise positive proposals.
- World schema 64, generator 25 and SQLite 31 continue from the parent branch.
