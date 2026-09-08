# Reserve incoming site freight

Related: #391 and #471 under #396. Base: draft #517 at `e6747b3`.

Site supply plans count goods already travelling or waiting to unload. The planner subtracts those quantities from the requested supply and reserves their future store slots across all goods. Lost and completed loads release their reservation. Actual unloading still uses physical room at arrival.

The current royal fleet has one carriage per kingdom. A town can change crowns while an earlier carriage is still bringing its supplies. The new owner's carriage then plans against both the store and the earlier load. This also provides the same incoming-goods rule for the planned shared county-cart pool.

A valid saved fixture starts a Tools shipment, moves the home town to an allied crown, and lets the new owner's carriage plan before the first load arrives. Schema 68 orders another Tool. Schema 69 orders the four Wheat still needed. Both loads arrive through the existing carriage timing and store-transfer path. Saved older duplicate loads retain their actual cargo through upgrade.

Simulation schema 69 gates the planning rule. Generator 25, SQLite schema 31 and saved fields remain unchanged. JSON protocol 6 uses the existing production and freight counters.

## Evidence and scope

The planning tests cover travelling, blocked, lost and arrived loads; another destination; partial orders; store space reserved by a different good; partially unloaded cargo; and readonly plans. The full delivery fixture checks current and historical behavior, save/reload and real arrivals after the ownership change.

The earlier 40-year opened-site trace showed Stag's Mill short of Wheat, Crown Forge short of Iron, and Ashfield Farm short of Tools. Their home towns also lacked those goods while routes were open. Broader town supply and trade funding remain the next production issue. This change addresses duplicate orders and store allocation in the transport layer.

All 84 local tests passed. Recorded on clean Release commit `0fbd2ec799d7fdb3dcb1dc60af4625bf74ddf928`: four policy/site cases with two matching runs each and 41 checkpoints per run. Every site stock and condition equation passes at each checkpoint. All 1,040 historical annual hashes match the parent across the versions in `parity.json`. The manifest and outcomes pin the capture provenance. The ownership-change fixture supplies the focused before/after behavior evidence.
